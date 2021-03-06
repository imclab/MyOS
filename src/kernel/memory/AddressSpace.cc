#include <memory/AddressSpace.h>

#include <core/Scheduler.h>
#include <core/Debug.h>
#include <memory/FrameAlloc.h>
#include <memory/Memory.h>
#include <alloc/malloc.h>
#include <tty/Escape.h>
#include <core/CPU.h>
#include <kutil.h>
#include <string.h>

static AddressSpace __kernelSpace;

AddressSpace* AddressSpace::kernelSpace = &__kernelSpace;
AddressSpace* AddressSpace::current = NULL;

#define ADDR_TRAP 0xbadc0de


static page_tree_node_t* allocate_node() {
    return (page_tree_node_t*)kvalloc(sizeof(page_tree_node_t));
}

static void initialize_node_entry(page_tree_node_entry_t* entry) {
    entry->present = 0;
    entry->rw = 1;
    entry->user = 1;
    entry->unused = 0;
    entry->address = ADDR_TRAP; // trap
}

static void initialize_node(page_tree_node_t* node) {
    for (int idx = 0; idx < 512; idx++) {
        initialize_node_entry(&(node->entries[idx]));
        node->entriesVirtual[idx] = 0;
        node->entriesAttrs[idx] = 0;
        node->entriesNames[idx] = 0;
    }
}


static page_tree_node_t* node_get_child(page_tree_node_t* node, uint64_t idx, bool create) {
    if (!node->entries[idx].present || node->entries[idx].address == ADDR_TRAP) {
        if (!create) {
            return NULL;
        }
        page_tree_node_t* child = allocate_node();
        initialize_node(child);
        node->entries[idx].present = 1;
        node->entriesVirtual[idx] = child;
        if (AddressSpace::current)
            node->entries[idx].address = AddressSpace::current->getPhysicalAddress((uint64_t)child) / KCFG_PAGE_SIZE; 
        else
            node->entries[idx].address = (uint64_t)child / KCFG_PAGE_SIZE; 

        return child;
    }
    return node->entriesVirtual[idx];
}


static void copy_page_physical(uint64_t src, uint64_t dst) {
    AddressSpace::current->mapPage(AddressSpace::current->getPage(KCFG_TEMP_PAGE_1, true), src, PAGEATTR_SHARED);
    AddressSpace::current->mapPage(AddressSpace::current->getPage(KCFG_TEMP_PAGE_2, true), dst, PAGEATTR_SHARED);
    AddressSpace::current->activate();
    memcpy((void*)KCFG_TEMP_PAGE_2, (void*)KCFG_TEMP_PAGE_1, KCFG_PAGE_SIZE);
    //AddressSpace::current->releasePage(AddressSpace::current->getPage(KCFG_TEMP_PAGE_1, false));
    //AddressSpace::current->releasePage(AddressSpace::current->getPage(KCFG_TEMP_PAGE_2, false));
}



// -------------------------------


AddressSpace::AddressSpace() {
    root = NULL;
}

AddressSpace::~AddressSpace() {
    page_tree_node_t* node = getRoot();

    for (int i = 0; i < 512; i++) { // PML4s
        if (node->entries[i].present) {
            page_tree_node_t* pml4 = node->entriesVirtual[i];
            for (int j = 0; j < 512; j++) { // PDPTs
                if (pml4->entries[j].present) {
                    page_tree_node_t* pd = pml4->entriesVirtual[j];
                    for (int l = 0; l < 512; l++) { // PTs
                        if (pd->entries[l].present) {
                            page_tree_node_t* pt = pd->entriesVirtual[l];
                            for (int m = 0; m < 512; m++) { // Pages
                                if (pt->entries[m].present) {
                                    uint64_t addr = i;
                                    addr = addr * 512 + j;
                                    addr = addr * 512 + l;
                                    addr = addr * 512 + m;
                                    addr *= KCFG_PAGE_SIZE;

                                    if (addr >=       800000000000) {
                                        addr += 0xffff000000000000;
                                    }

                                    page_descriptor_t oldPage = getPage(addr, false);
                                    
                                    if (PAGEATTR_IS_COPY(*oldPage.attrs))
                                        releasePage(oldPage);
                                }
                            }
                            delete pt;
                        }
                    }
                    delete pd;
                }
            }
            delete pml4;
        }
    }
    delete root;
}

void AddressSpace::initEmpty() {
    if (!getRoot())
        setRoot(allocate_node());
    initialize_node(getRoot());
}

void AddressSpace::activate() {
    if (AddressSpace::current) {
        //klog('t',"Switching address space: %16lx",AddressSpace::current->getPhysicalAddress((uint64_t)root));
        CPU::setCR3(AddressSpace::current->getPhysicalAddress((uint64_t)root));
    }
    else
        CPU::setCR3((uint64_t)root);
    current = this;
}

page_tree_node_t* AddressSpace::getRoot() {
    return root;
}

void AddressSpace::setRoot(page_tree_node_t* r) {
    root = r;
}

page_descriptor_t AddressSpace::getPage(uint64_t virt, bool create) {
    page_descriptor_t d;
    page_tree_node_t* root = getRoot();
    d.pageVAddr = virt;

    // Skip memory hole
    uint64_t fixedVirt = virt;

    if (fixedVirt >= 0xffff800000000000) {
        fixedVirt -= 0xffff000000000000;
    }

    uint64_t page = fixedVirt / KCFG_PAGE_SIZE; // page index
    uint64_t indexes[4] = {
        page / 512 / 512 / 512 % 512,
        page / 512 / 512 % 512,
        page / 512 % 512,
        page % 512,
    };

    for (int i = 0; i < 3; i++) {
        //if (Debug::tracingOn)
            //klog('t', "ngc root %lx index %i", root, indexes[i]);

        if ((uint64_t)root / KCFG_PAGE_SIZE == ADDR_TRAP) {
            klog('e', "NODE ADDRESS WAS TRAP @ getpage(%lx)", virt);
            klog_flush();
            for(;;);
        }

        root = node_get_child(root, indexes[i], create);

        if (root == NULL && !create) {
            d.entry = 0;
            return d;
        }
    }

    root->entriesVirtual[page % 512] = (page_tree_node_t*)virt;

    d.entry = &(root->entries[page % 512]);
    d.attrs = &(root->entriesAttrs[page % 512]);
    d.vAddr = &(root->entriesVirtual[page % 512]);
    d.name  = &(root->entriesNames[page % 512]);

    return d;
}

uint64_t AddressSpace::getPhysicalAddress(uint64_t virt) {
    return getPage(virt, false).entry->address * KCFG_PAGE_SIZE + (virt % KCFG_PAGE_SIZE);
}

page_descriptor_t AddressSpace::mapPage(page_descriptor_t page, uint64_t phy, uint8_t attrs) {
//    klog('t', "Mapping page: %lx -> %lx", page.pageVAddr, phy);
    FrameAlloc::get()->markAllocated(phy / KCFG_PAGE_SIZE);
    page.entry->present = true;
    page.entry->user = true;
    page.entry->rw = true;
    page.entry->address = phy / KCFG_PAGE_SIZE;
    *(page.attrs) = attrs;
    *(page.vAddr) = (page_tree_node_t*)page.pageVAddr;
    return page;
}

void AddressSpace::namePage(page_descriptor_t page, char* name) {
    *(page.name) = name;
}

page_descriptor_t AddressSpace::allocatePage(page_descriptor_t page, uint8_t attrs) {
    if (!page.entry->present) {
        uint64_t frame = FrameAlloc::get()->allocate();
        mapPage(page, frame * KCFG_PAGE_SIZE, attrs);
    }
    return page;
}

void AddressSpace::allocateSpace(uint64_t base, uint64_t size, uint8_t attrs) {
    uint64_t top = base + size;
    base = base / KCFG_PAGE_SIZE * KCFG_PAGE_SIZE;
    top = (top + KCFG_PAGE_SIZE - 1) / KCFG_PAGE_SIZE * KCFG_PAGE_SIZE;
    size = top - base;
    #ifdef KCFG_ENABLE_TRACING
        klog('t', "Allocating %lx bytes at %lx", size, base);klog_flush();
    #endif
    for (uint64_t v = base; v < base + size; v += KCFG_PAGE_SIZE) {
        allocatePage(getPage(v, true), attrs);
    }
}

void AddressSpace::writePage(void* buf, uint64_t base, uint64_t size) {
    uint64_t ptr = base / KCFG_PAGE_SIZE * KCFG_PAGE_SIZE;
    uint64_t bufptr = (uint64_t)buf / KCFG_PAGE_SIZE * KCFG_PAGE_SIZE;
    uint64_t offset = (uint64_t)base - ptr;
    uint64_t bufoffset = (uint64_t)buf - bufptr;

    klog('t', "Copying %lx bytes to %lx+%lx", size, ptr, offset);

    AddressSpace::current->mapPage(
        AddressSpace::current->getPage(KCFG_TEMP_PAGE_1, true), 
        AddressSpace::current->getPhysicalAddress(bufptr), 
        PAGEATTR_SHARED
    );
    AddressSpace::current->mapPage(
        AddressSpace::current->getPage(KCFG_TEMP_PAGE_2, true), 
        getPhysicalAddress(ptr), 
        PAGEATTR_SHARED
    );
    AddressSpace::current->activate();

    memcpy(
        (void*)(KCFG_TEMP_PAGE_2 + offset), 
        (void*)(KCFG_TEMP_PAGE_1 + bufoffset), 
        size
    );
}

void AddressSpace::write(void* buf, uint64_t base, uint64_t size) {
    uint64_t srcPage = -1;
    uint64_t dstPage = -1;

    uint64_t src = (uint64_t)buf, dst = base;

    klog('t', "Copying %lx bytes %lx -> %lx", size, buf, base);

    for (uint64_t v = 0; v < size; v++) {
        if (PAGEALIGN(src) != srcPage) {
            srcPage = PAGEALIGN(src);
            AddressSpace::current->mapPage(
                AddressSpace::current->getPage(KCFG_TEMP_PAGE_1, true), 
                AddressSpace::current->getPhysicalAddress(srcPage), 
                PAGEATTR_SHARED
            );
            AddressSpace::current->activate();
        }

        if (PAGEALIGN(dst) != dstPage) {
            dstPage = PAGEALIGN(dst);
            AddressSpace::current->mapPage(
                AddressSpace::current->getPage(KCFG_TEMP_PAGE_2, true), 
                getPhysicalAddress(dstPage), 
                PAGEATTR_SHARED
            );
            AddressSpace::current->activate();
        }

        uint64_t srcOffset = src - srcPage;
        uint64_t dstOffset = dst - dstPage;

        *(uint8_t*)(KCFG_TEMP_PAGE_2 + dstOffset) = *(uint8_t*)(KCFG_TEMP_PAGE_1 + srcOffset);

        src++;
        dst++;
    }
}

void AddressSpace::releasePage(page_descriptor_t page) {
    if (page.entry->present) {
        FrameAlloc::get()->release(page.entry->address);
        initialize_node_entry(page.entry);
    }
}

void AddressSpace::releaseSpace(uint64_t base, uint64_t size) {
    uint64_t top = base + size;
    base = base / KCFG_PAGE_SIZE * KCFG_PAGE_SIZE;
    top = (top + KCFG_PAGE_SIZE - 1) / KCFG_PAGE_SIZE * KCFG_PAGE_SIZE;
    size = top - base;
    #ifdef KCFG_ENABLE_TRACING
        klog('t', "Releasing %lx bytes at %lx", size, base);
    #endif
    for (uint64_t v = base; v < base + size; v += KCFG_PAGE_SIZE) {
        releasePage(getPage(v, true));
    }
}

AddressSpace* AddressSpace::clone() {
    CPU::CLI();
    Scheduler::get()->pause();

    AddressSpace* result = new AddressSpace();
    result->initEmpty();
    klog('t', "Cloning address space from %lx (%lx) into %lx (%lx)", this, getRoot(), result, result->getRoot());

    page_tree_node_t* node = getRoot();

    for (int i = 0; i < 512; i++) { // PML4s
        //klog('w', "%i", i);klog_flush();
        if (node->entries[i].present) {
            page_tree_node_t* pml4 = node->entriesVirtual[i];
            
            for (int j = 0; j < 512; j++) { // PDPTs
                if (pml4->entries[j].present) {
                    page_tree_node_t* pd = pml4->entriesVirtual[j];

                    for (int l = 0; l < 512; l++) { // PTs
                        if (pd->entries[l].present) {
                            page_tree_node_t* pt = pd->entriesVirtual[l];

                            for (int m = 0; m < 512; m++) { // Pages
                                if (pt->entries[m].present) {
                                    uint64_t addr = i;
                                    addr = addr * 512 + j;
                                    addr = addr * 512 + l;
                                    addr = addr * 512 + m;
                                    addr *= KCFG_PAGE_SIZE;


                                    if (addr >=       800000000000) {
                                        addr += 0xffff000000000000;
                                    }

                                    page_descriptor_t oldPage = getPage(addr, false);
                                    //klog('t', "Processing page %lx == %lx", addr, oldPage.entry->address);
                                    
                                    if (!oldPage.entry) {
                                        klog('e', "NULL PAGE @ %lx", addr);
                                        klog_flush();
                                        for(;;);
                                    }
                                    if (oldPage.entry->address == ADDR_TRAP) {
                                        klog('e', "TRAP PAGE @ %lx", addr);
                                        klog_flush();
                                        for(;;);
                                    }

                                    if (PAGEATTR_IS_SHARED(*oldPage.attrs)) {
                                        //klog('t', "Mapping this page as shared");
                                        page_descriptor_t page = result->getPage(addr, true);

                                        *(page.vAddr) = *oldPage.vAddr;
                                        *(page.entry) = *oldPage.entry;
                                        *(page.attrs) = *oldPage.attrs;
                                        *(page.name) = *oldPage.name;

                                        if (PAGEATTR_IS_COPY(*oldPage.attrs)) {
                                            //klog('t', "Copying phy page (%lx -> %lx)",
                                            //    oldPage.entry->address * KCFG_PAGE_SIZE,
                                            //    page.entry->address * KCFG_PAGE_SIZE);
                                            page.entry->present = false;
                                            result->allocatePage(page, *oldPage.attrs);
                                            copy_page_physical(
                                                oldPage.entry->address * KCFG_PAGE_SIZE,
                                                page.entry->address * KCFG_PAGE_SIZE
                                            );
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    klog('t', "Cloned address space into %lx", result);
    CPU::STI();

    return result;
}

void AddressSpace::release() {    
}



void AddressSpace::recursiveDump(page_tree_node_t* node, int level) {
    static uint64_t skips[4] = {
        512 * 512 * 512,
        512 * 512,
        512
    };

    static uint64_t addr, startPhy, startVirt, lastVirt, lastPhy, len;
    static bool started = false;

    if (level == 0) {
        addr = 0;
        startPhy = 0;
        startVirt = 0;
        len = 0;
        started = false;
        lastVirt = -1;
    }

    for (int i = 0; i < 512; i++) {
        if (addr >= 0x0000800000000000 && addr < 0xffff800000000000)
            addr += 0xffff000000000000;

        if (level == 3) {
            if (node->entries[i].present) {
                startVirt = addr;
                startPhy = node->entries[i].address * KCFG_PAGE_SIZE;
                if ((startVirt != lastVirt + KCFG_PAGE_SIZE) || (startPhy != lastPhy + KCFG_PAGE_SIZE)) {
                    if (started)
                        klog('i', "%s%16lx    %16lx  %lx", 
                            Escape::C_GRAY, 
                            lastVirt + 0xfff, 
                            lastPhy + 0xfff, 
                            len * KCFG_PAGE_SIZE);
                    started = true;

                    uint8_t attrs = node->entriesAttrs[i];
                    klog('i', "%s%16lx -> %16lx %s [%s %s %s] %s", Escape::C_B_GRAY, startVirt, startPhy, 
                        Escape::C_GRAY,
                        PAGEATTR_IS_SHARED(attrs) ? "SHR": "---",
                        PAGEATTR_IS_USER(attrs)   ? "USR": "KRN",
                        PAGEATTR_IS_COPY(attrs)   ? "CPY": "---",
                        node->entriesNames[i] ? node->entriesNames[i] : "---"
                    );
                    klog_flush();
                    len = 1;
                } else {
                    len++;
                }

                lastVirt = startVirt;
                lastPhy = startPhy;
            }
            addr += KCFG_PAGE_SIZE;
        } else {
            if (node->entries[i].present) {
                recursiveDump(node_get_child(node, i, false), level + 1);
            } else {
                addr += skips[level] * KCFG_PAGE_SIZE;
            }
        }
    }
}

void AddressSpace::dump() {
    klog('w', "Dumping address space %16lx", this);
    recursiveDump(root, 0);
}
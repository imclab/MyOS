#ifndef UTIL_LINKEDLIST_H
#define UTIL_LINKEDLIST_H

template <class T>
class LinkedListEntry {
public:
    T value;
    LinkedListEntry<T>* next;
};



template <class T>
class LinkedListIter {
public:
    LinkedListIter<T>(LinkedListEntry<T>* l) {
        c = l;
    }

    void next() {
        c = c->next;
    }

    int end() {
        return (c == 0) or (c->next == 0);
    }

    T get() {
        return c->value;
    }
private:
    LinkedListEntry<T>* c;
};



template <class T>
class LinkedList {
public:
    LinkedList<T>() {
        root = 0;
    }

    LinkedListEntry<T> getRoot() {
        return root;
    }

    LinkedListIter<T>* iter() {
        LinkedListIter<T>* i = new LinkedListIter<T>*();
        i->init(this);
        return i;
    }

    T get(int idx) {
        LinkedListEntry<T>* p = root;
        for (int i = 0; i < idx; i++)
            p = p->next;
        return p->value;
    }

    int length() {
        LinkedListEntry<T>* p = root;
        int r = 0;
        for (; p; r++)
            p = p->next;
        return r;
    }

    void insert(T val, int idx) {
        LinkedListEntry<T>* i = new LinkedListEntry<T>();
        i->value = val;
        i->next = 0;
        if (!root)
            root = i;
        else if (idx == 0) {
            i->next = root;
            root = i;
        } else {
            LinkedListEntry<T>* p = root;
            for (int ii = 0; ii < idx-1; ii++)
                p = p->next;
            LinkedListEntry<T>* tmp = p->next;
            p->next = i;
            i->next = tmp;
        }
    }

    T remove(int idx) {
        T ret = (T)NULL;
        if (!root)
            return ret;
        else if (idx == 0) {
            ret = root->value;
            root = root->next;
        } else {
            LinkedListEntry<T>* p = root;
            for (int i = 0; i < idx-1; i++)
                p = p->next;

            LinkedListEntry<T>* tmp = p->next;
            p->next = p->next->next;
            ret = tmp->value;
            delete p->next;
        }
        return ret;
    }


    void insertLast(T val) {
        insert(val, length());
    }


private:
    LinkedListEntry<T>* root;
};


#endif

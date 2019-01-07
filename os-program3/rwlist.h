#pragma once
#include <pthread.h>

//a thread-safe (concurrent) sorted linked list of integers, which should use readers/writer locking.
class rwlist {
	/// a node consists of a value and a pointer to another node
	struct Node {
		int value;
		Node* next;
	};

	Node* constructNode(int key) {
		Node* obj = new Node;
		obj -> value = key;
		obj -> next = NULL;
		return obj;
	}

	/// The head of the list is referenced by this pointer
	Node* head;
	mutable pthread_rwlock_t rwlock;
public:
	rwlist(int) {
		this -> head = NULL;
		pthread_rwlock_init(&this -> rwlock, NULL);
	}
	~rwlist() {
		Node* runner = this -> head;
		while (runner != NULL) {
			Node* temp = runner -> next;
			delete runner;
			runner = temp;
		}
		pthread_rwlock_destroy(&this -> rwlock);
	}

	/// insert *key* into the linked list if it doesn't already exist; return
	/// true if the key was added successfully.
	bool insert(int key) {
		pthread_rwlock_wrlock(&this -> rwlock);
		//if the linked list is empty, create a new node and return true
		if (head == NULL) {
			head = constructNode(key);
			pthread_rwlock_unlock(&this -> rwlock);
			return true;
		}
		//if the head of list has value of the key, return false
		else if (head -> value == key) {
			pthread_rwlock_unlock(&this -> rwlock);
			return false;
		}
		//if the head of list's value is greater than the key, update head of linked list and return true
		else if (head -> value > key) {
			Node* obj = constructNode(key);
			obj -> next = head;
			head = obj;
			pthread_rwlock_unlock(&this -> rwlock);
			return true;
		}

		Node* runner = head;
		while (runner -> next != NULL && runner -> next -> value <= key) {
			if (runner -> next -> value == key) {
				pthread_rwlock_unlock(&this -> rwlock);
				return false;
			}
			runner = runner -> next;
		}
		Node* obj = constructNode(key);
		obj -> next = runner -> next;
		runner -> next = obj;
		pthread_rwlock_unlock(&this -> rwlock);
		return true;
	}
	/// remove *key* from the list if it was present; return true if the key
	/// was removed successfully.
	bool remove(int key) {
		pthread_rwlock_wrlock(&this -> rwlock);
		//if the linked list is empty, return false
		if (head == NULL) {
			pthread_rwlock_unlock(&this -> rwlock);
			return false;
		}
		//if the value is at the head of list, update the head value
		else if (head -> value == key) {
			Node* obj = head;
			head = head -> next;
			delete obj;
			pthread_rwlock_unlock(&this -> rwlock);
			return true;
		}
		Node* runner = head;
		while (runner -> next != NULL && runner -> next -> value <= key) {
			if (runner -> next -> value == key) {
				Node* obj = runner -> next;
				runner -> next = runner -> next -> next;
				delete obj;
				pthread_rwlock_unlock(&this -> rwlock);
				return true;
			}
			runner = runner -> next;
		}
		//return false if the the target value was never found
		pthread_rwlock_unlock(&this -> rwlock);
		return false;
	}
	/// return true if *key* is present in the list, false otherwise
	bool lookup(int key) const {
		pthread_rwlock_rdlock(&this -> rwlock);
		//if the linked list is empty, return false
		if (head == NULL) {
			pthread_rwlock_unlock(&this -> rwlock);
			return false;
		}
		//traverse the linked list and check for the value of the key
		Node* runner = head;
		while (runner != NULL && runner -> value <= key) {
			if (runner -> value == key) {
				pthread_rwlock_unlock(&this -> rwlock);
				return true;
			}
			runner = runner -> next;
		}
		//return false if the the target value was never found
		pthread_rwlock_unlock(&this -> rwlock);
		return false;
	}

	//The following are not tested by the given tester but are required for grading
	//No locks are required for these.
	size_t getSize() const {
		//traverse the linked list and update the count
		Node* runner = head;
		size_t count = 0;
		while (runner != NULL) {
			count++;
			runner = runner -> next;
		}
		return count;
	}
	int getElement(size_t idx) const {
		//traverse the linked list until we reach the index or the value of runner is NULL
		Node* runner = head;
		for (int i = 0; i < idx; i++) {
			if (runner == NULL) {
				return -1;
			}
			runner = runner -> next;
		}
		if (runner != NULL) {
			return runner -> value;
		}
		return -1;
	}

	size_t getBucketSize(size_t bucket) const
	{
		//Do not edit this
		return 0;
	}
	int getElement(size_t bucket, size_t idx) const
	{
		//Do not edit this
		return 0;
	}
};

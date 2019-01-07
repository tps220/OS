#pragma once
#include <mutex>

//a thread-safe (concurrent) hash table of integers, implemented as an array of linked lists.  In
//this implementation, each list should have a "sentinel" node that
//contains the lock, so we can't just reuse the clist implementation
class shash {
	struct Node {
		int value;
		Node* next;
		Node(int val): value(val), next(NULL) {}
	};

	struct SentinelNode {
		mutable Node* next;
		char padding[16];
		mutable std::mutex mutex;
		SentinelNode() : next(NULL) {}
	};

	void destroyList(const SentinelNode* root) {
		Node* runner = root -> next;
		while (runner != NULL) {
			Node* temp = runner -> next;
			delete runner;
			runner = temp;
		}
		delete root;
	}

	inline size_t getKey(int val) const {
		return val % size;
	}

	//data fields
	const SentinelNode** buckets;
	size_t size;
public:
	//constructor
	shash(unsigned _buckets): buckets(new const SentinelNode*[_buckets]) {
		for (int i = 0; i < _buckets; i++) {
			this -> buckets[i] = new SentinelNode();
		}
		this -> size = _buckets;
	}
	//destructor
	~shash() {
		for (int i = 0; i < size; i++) {
			destroyList(buckets[i]);
		}
		delete [] buckets;
	}

	//insert *key* into the appropriate linked list if it doesn't already exist;
	//return true if the key was added successfully.
	bool insert(int key) {
		const SentinelNode* root = this -> buckets[getKey(key)];
		root -> mutex.lock();
		//if the linked list is empty, create a new node and return true
		if (root -> next == NULL) {
			root -> next = new Node(key);
			root -> mutex.unlock();
			return true;
		}
		//if the head of list has value of the key, return false
		else if (root -> next -> value == key) {
			root -> mutex.unlock();
			return false;
		}
		//if the head of list's value is greater than the key, update head of linked list and return true
		else if (root -> next -> value > key) {
			Node* obj = new Node(key);
			obj -> next = root -> next;
			root -> next = obj;
			root -> mutex.unlock();
			return true;
		}

		Node* runner = root -> next;
		while (runner -> next != NULL && runner -> next -> value <= key) {
			if (runner -> next -> value == key) {
				root -> mutex.unlock();
				return false;
			}
			runner = runner -> next;
		}
		Node* obj = new Node(key);
		obj -> next = runner -> next;
		runner -> next = obj;
		root -> mutex.unlock();
		return true;
	}

	//remove *key* from the appropriate list if it was present;
	//return true if the key was removed successfully.
	bool remove(int key) {
		const SentinelNode* root = this -> buckets[getKey(key)];
		root -> mutex.lock();
		//if the linked list is empty, return false
		if (root -> next == NULL) {
			root -> mutex.unlock();
			return false;
		}
		//if the value is at the head of list, update the head value
		else if (root -> next -> value == key) {
			Node* obj = root -> next;
			root -> next = root -> next -> next;
			delete obj;
			root -> mutex.unlock();
			return true;
		}
		Node* runner = root -> next;
		while (runner -> next != NULL && runner -> next -> value <= key) {
			if (runner -> next -> value == key) {
				Node* obj = runner -> next;
				runner -> next = runner -> next -> next;
				delete obj;
				root -> mutex.unlock();
				return true;
			}
			runner = runner -> next;
		}
		//return false if the the target value was never found
		root -> mutex.unlock();
		return false;
	}

	//return true if *key* is present in the appropriate list, false otherwise
	bool lookup(int key) const {
		const SentinelNode* root = this -> buckets[getKey(key)];
		root -> mutex.lock();

		//traverse the linked list and check for the value of the key
		Node* runner = root -> next;
		while (runner != NULL && runner -> value <= key) {
			if (runner -> value == key) {
				root -> mutex.unlock();
				return true;
			}
			runner = runner -> next;
		}
		//return false if the the target value was never found
		root -> mutex.unlock();
		return false;
	}

	//The following are not tested by the given tester but are required for grading
	//No locks are required for these.

	//This refers to the number of buckets not the total number of elements.
	size_t getSize() const {
		return this -> size;
	}

	//This refers to the number of elements in a bucket, not the sentinel node.
	size_t getBucketSize(size_t bucket) const {
		//traverse the linked list and update the count
		if (bucket >= size) {
			return 0;
		}
		Node* runner = this -> buckets[bucket] -> next;
		size_t count = 0;
		while (runner != NULL) {
			count++;
			runner = runner -> next;
		}
		return count;
	}
	int getElement(size_t bucket, size_t idx) const {
		//traverse the linked list until we reach the index or the value of runner is NULL
		if (bucket >= size) {
			return -1;
		}
		Node* runner = this -> buckets[bucket] -> next;
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

	int getElement(size_t idx) const
	{
		//Do not edit this
		return 0;
	}
};

#pragma once
#include <vector>
#include <mutex>
/// TODO: complete this implementation of a thread-safe (concurrent) hash
///       table of integers, implemented as an array of linked lists.  In
///       this implementation, each list should have a "sentinel" node that
///       contains the lock, so we can't just reuse the clist implementation
class shash
{
	//a Node consists of a value and a pointer to another node
	struct Node
	{
		int value;
		mutable std::mutex mutex;
		Node* next;
		Node(int val): value(val), next(NULL) {}
	};

	void destroyList(Node* runner) {
		while (runner != NULL) {
			Node* temp = runner -> next;
			delete runner;
			runner = temp;
		}
	}

	size_t getKey(int val) const {
		return val % size;
	}

	//data fields
	std::vector<Node*> buckets;
	size_t size;
public:
	//constructor
	shash(unsigned _buckets) {
		for (int i = 0; i < _buckets; i++) {
			this -> buckets.push_back(new Node(0));
		}
		this -> size = _buckets;
	}
	//destructor
	~shash() {
		for (int i = 0; i < size; i++) {
			destroyList(buckets[i]);
		}
	}

	/// insert *key* into the appropriate linked list if it doesn't already
	/// exist; return true if the key was added successfully.
	bool insert(int key)
	{
		size_t location = getKey(key);
		Node* runner = this -> buckets[location];
		runner -> mutex.lock();
		//if an empty list, return false
		if (runner -> next == NULL) {
			runner -> next = new Node(key);
			runner -> mutex.unlock();
			return true;
		}
		Node* prev = runner;
		runner -> next -> mutex.lock();
		runner = runner -> next;
		while (runner -> next != NULL && runner -> value < key) {
			prev -> mutex.unlock();
			prev = runner;
			runner -> next -> mutex.lock();
			runner = runner -> next;
		}
		if (runner && runner -> value == key) {
			runner -> mutex.unlock();
			prev -> mutex.unlock();
			return false;
		}
		else if (runner -> next == NULL && runner -> value < key) {
			runner -> next = new Node(key);
			runner -> mutex.unlock();
			prev -> mutex.unlock();
			return true;
		}
		Node* node = new Node(key);
		node -> next = runner;
		prev -> next = node;
		runner -> mutex.unlock();
		prev -> mutex.unlock();
		return true;
	}
	/// remove *key* from the appropriate list if it was present; return true
	/// if the key was removed successfully.
	bool remove(int key)
	{
		size_t location = getKey(key);
		Node* runner = this -> buckets[location];
		runner -> mutex.lock();
		//if an empty list, return false
		if (runner -> next == NULL) {
			runner -> mutex.unlock();
			return false;
		}
		Node* prev = runner;
		runner -> next -> mutex.lock();
		runner = runner -> next;
		while (runner -> next != NULL && runner -> value < key) {
			prev -> mutex.unlock();
			prev = runner;
			runner -> next -> mutex.lock();
			runner = runner -> next;
		}
		if (runner -> value == key) {
			prev -> next = runner -> next;
			runner -> mutex.unlock();
			prev -> mutex.unlock();
			delete runner;
			return true;
		}
		runner -> mutex.unlock();
		prev -> mutex.unlock();
		return false;
	}
	/// return true if *key* is present in the appropriate list, false
	/// otherwise
	bool lookup(int key) const
	{
		size_t location = getKey(key);
		Node* runner = this -> buckets[location];
		runner -> mutex.lock();
		//if an empty list, return false
		if (runner -> next == NULL) {
			runner -> mutex.unlock();
			return false;
		}
		Node* prev = runner;
		runner -> next -> mutex.lock();
		runner = runner -> next;
		while (runner -> next != NULL && runner -> value < key) {
			prev -> mutex.unlock();
			prev = runner;
			runner -> next -> mutex.lock();
			runner = runner -> next;
		}
		if (runner -> value == key) {
			prev -> mutex.unlock();
			runner -> mutex.unlock();
			return true;
		}
		prev -> mutex.unlock();
		runner -> mutex.unlock();
		return false;
	}

	//The following are not tested by the given tester but are required for grading
	//No locks are required for these.

	//This refers to the number of buckets not the total number of elements.
	size_t getSize() const
	{
		return this -> size;
	}

	//This refers to the number of elements in a bucket, not the sentinel node.
	size_t getBucketSize(size_t bucket) const
	{
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
	int getElement(size_t bucket, size_t idx) const
	{
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

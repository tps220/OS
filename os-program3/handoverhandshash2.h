#pragma once
#include "pthread.h"
/// TODO: complete this implementation of a thread-safe (concurrent) hash
///       table of integers, implemented as an array of linked lists.  In
///       this implementation, each list should have a "sentinel" node that
///       contains the lock, so we can't just reuse the clist implementation.
///       In addition, the API now allows for multiple keys on each
///       operation.
class shash2
{
	//a Node consists of a value and a pointer to another node
	struct Node
	{
		int value;
		mutable std::mutex mutex;
		Node* next;
		Node(int val): value(val), next(NULL) {}
	};

	struct Parameters {
		int value;
		int retval;
		Node* root;
	};

	void destroyList(Node* runner) {
		while (runner != NULL) {
			Node* temp = runner -> next;
			delete runner;
			runner = temp;
		}
	}

	inline size_t getKey(int val) const {
		return val % size;
	}

	void swap(int &a, int &b) {
		int temp = a;
		a = b;
		b = temp;
	}

	int partition(int *keys, int left, int right) {
		int mid = (left + right) / 2;
		if (keys[mid] < keys[left]) {
			swap(keys[mid], keys[left]);
		}
		if (keys[right] < keys[left]) {
			swap(keys[right], keys[left]);
		}
		if (keys[right] < keys[mid]) {
			swap(keys[right], keys[mid]);
		}
		swap(keys[mid], keys[right]);
		int pivot = keys[right];

		int placePivot = -1;
		for (int i = left; i < right; i++) {
			if (keys[i] <= pivot) {
				placePivot++;
				swap(keys[i], keys[placePivot]);
			}
		}
		swap(keys[placePivot + 1], keys[right]);
		return placePivot + 1;
	}

	void sort(int* keys, int left, int right) {
		if (left < right) {
			int p = partition(keys, left, right);
			sort(keys, left, p - 1);
			sort(keys, p + 1, right);
		}
	}

	std::vector<Node*> buckets;
	size_t size;
public:
	shash2(unsigned _buckets) {
		for (int i = 0; i < _buckets; i++) {
			buckets.push_back(new Node(0));
		}
		size = _buckets;
	}

	~shash2() {
		for (int i = 0; i < size; i++) {
			destroyList(buckets[i]);
		}
	}
	/// insert /num/ values from /keys/ array into the hash, and return the
	/// success/failure of each insert in /results/ array.
	void insert(int* keys, bool* results, int num) {
		void* (*func)(void*);
		func = &this -> insertHelper;
		pthread_t thread_id[num];
		struct Parameters parameters[num];
		for (int i = 0; i < num; i += 8) {
			for (int j = i; j < i + 8 && j < num; j++) {
				parameters[j].value = keys[j];
				parameters[j].retval = 0;
				parameters[j].root = this -> buckets[getKey(keys[j])];
				pthread_create(&thread_id[j], NULL, func, &parameters[j]);
			}
			for (int j = i; j < i + 8 && j < num; j++) {
				pthread_join(thread_id[j], NULL);
				results[j] = parameters[j].retval;
			}
		}
	}

	static void* insertHelper(void* input) {
		struct Parameters* parameters = (struct Parameters*)input;
		int key = parameters -> value;
		Node* runner = parameters -> root;
		runner -> mutex.lock();
		//if an empty list, return false
		if (runner -> next == NULL) {
			runner -> next = new Node(key);
			runner -> mutex.unlock();
			parameters -> retval = 1;
			return parameters;
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
			runner -> mutex.unlock();
			prev -> mutex.unlock();
			parameters -> retval = 0;
			return parameters;
		}
		else if (runner -> next == NULL && runner -> value < key) {
			runner -> next = new Node(key);
			runner -> mutex.unlock();
			prev -> mutex.unlock();
			parameters -> retval = 1;
			return parameters;
		}
		Node* node = new Node(key);
		node -> next = runner;
		prev -> next = node;
		runner -> mutex.unlock();
		prev -> mutex.unlock();
		parameters -> retval = 0;
		return parameters;
	}

	/// remove *key* from the list if it was present; return true if the key
	/// was removed successfully.
	void remove(int* keys, bool* results, int num)
	{
		void* (*func)(void*);
		func = &this -> removeHelper;
		pthread_t thread_id[num];
		struct Parameters parameters[num];
		for (int i = 0; i < num; i += 8) {
			for (int j = i; j < i + 8 && j < num; j++) {
				parameters[j].value = keys[j];
				parameters[j].retval = 0;
				parameters[j].root = this -> buckets[getKey(keys[j])];
				pthread_create(&thread_id[j], NULL, func, &parameters[j]);
			}
			for (int j = i; j < i + 8 && j < num; j++) {
				pthread_join(thread_id[j], NULL);
				results[j] = parameters[j].retval;
			}
		}
	}

	static void* removeHelper(void* input) {
		struct Parameters* parameters = (struct Parameters*)input;
		int key = parameters -> value;
		Node* runner = parameters -> root;
		runner -> mutex.lock();
		//if an empty list, return false
		if (runner -> next == NULL) {
			runner -> mutex.unlock();
			parameters -> retval = 0;
			return parameters;
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
			parameters -> retval = 1;
			return parameters;
		}
		runner -> mutex.unlock();
		prev -> mutex.unlock();
		parameters -> retval = 0;
		return parameters;
	}
	/// return true if *key* is present in the list, false otherwise
	void lookup(int* keys, bool* results, int num) const
	{
		void* (*func)(void*);
		func = &this -> lookupHelper;
		pthread_t thread_id[num];
		struct Parameters parameters[num];
		for (int i = 0; i < num; i += 8) {
			for (int j = i; j < i + 8 && j < num; j++) {
				parameters[j].value = keys[j];
				parameters[j].retval = 0;
				parameters[j].root = this -> buckets[getKey(keys[j])];
				pthread_create(&thread_id[j], NULL, func, &parameters[j]);
			}
			for (int j = i; j < i + 8 && j < num; j++) {
				pthread_join(thread_id[j], NULL);
				results[j] = parameters[j].retval;
			}
		}
	}

	void* lookupHelper(void* input) const {
		struct Parameters* parameters = (struct Parameters*)input;
		int key = parameters -> value;
		Node* runner = parameters -> root;
		runner -> mutex.lock();
		//if an empty list, return false
		if (runner -> next == NULL) {
			runner -> mutex.unlock();
			parameters -> retval = 0;
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
			parameters -> retval = 1;
			return true;
		}
		prev -> mutex.unlock();
		runner -> mutex.unlock();
		parameters -> retval = 0
		return false;
	}

	//The following are not tested by the given tester but are required for grading
	//No locks are required for these.

	//This refers to the number of buckets not the total number of elements.
	size_t getSize() const
	{
		return 0;
	}

	//This refers to the number of elements in a bucket, not the sentinel node.
	size_t getBucketSize() const
	{
		return 0;
	}
	int getElement(size_t bucket, size_t idx) const
	{
		return 0;
	}


	int getElement(size_t idx) const
	{
		//Do not edit this
		return 0;
	}
};

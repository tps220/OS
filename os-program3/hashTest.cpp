#include "shash.h"
#include <vector>
#include <set>
#include <unordered_map>
#include <iostream>
#include <algorithm>
using namespace std;

#define SIZE 512

int main(int argc, char** argv) {
  set<int> map;
  srand(time(NULL));
  for (int i = 0; i < 500000; i++) {
    map.insert(rand() % 500000 * 2);
  }
  shash hash(SIZE);

  //create insertions
  vector<int> keys(map.begin(), map.end());
  for (int i = 0; i < keys.size(); i++) {
    int place = rand() % keys.size();
    int temp = keys[i];
    keys[i] = keys[place];
    keys[place] = temp;
  }

  for (int i = 0; i < keys.size(); i++) {
    if (hash.insert(keys[i]) != true) {
      cout << "DID NOT INSERT PROPERLY" << endl;
    }
  }
  for (int i = 0; i < keys.size(); i++) {
    if (hash.insert(keys[i]) == true) {
      cout << "Inserted old vales / algorithm incorrect" << endl;
    }
  }

  //check insertions
  sort(keys.begin(), keys.end());
  for (int i = 0; i < keys.size(); i++) {
    if (hash.lookup(keys[i]) == false) {
      cout << "Did not find value: " << keys[i] << endl;
    }
  }

  unordered_map<int, int> old;
  for (int i = 0; i < keys.size(); i++) {
    int location = keys[i] % SIZE;
    int idx = 0;
    if (old.find(location) != old.end()) {
      idx = old[location] + 1;
      old[location]++;
    }
    else {
      old[location] = 0;
    }
    int val = hash.getElement(location, idx);
    if (val != keys[i]) {
      cout << "Got wrong value: " << val << " vs " << keys[i] << endl;
    }
  }

  //test remove on three important sections
  for (int i = 0; i < 10; i++) {
    if (hash.remove(keys[i]) != true) {
      cout << "invalid" << endl;
    }
  }
  for (int i = keys.size() - 10; i < keys.size(); i++) {
    if (hash.remove(keys[i]) != true) {
      cout << "invalid";
    }
  }
  for (int i = 0; i < 10; i++) {
    if (hash.remove(keys[i]) == true) {
      cout << "invalid" << endl;
    }
  }
  for (int i = keys.size() - 10; i < keys.size(); i++) {
    if (hash.remove(keys[i]) == true) {
      cout << "invalid";
    }
  }


  for (int i = 10; i < keys.size() - 10; i++) {
    if (hash.lookup(keys[i]) == false) {
      cout << "Did not remove properly" << endl;
    }
  }
  for (int i = 0; i < 10; i++) {
    if (hash.lookup(keys[i]) == true) {
      cout << "Did not remove properly" << endl;
    }
  }
  for (int i = keys.size() - 10; i < keys.size(); i++) {
    if (hash.lookup(keys[i]) == true) {
      cout << "Did not remove properly" << endl;
    }
  }

  //ensure size is the same
  int totalSize = 0;
  for (int i = 0; i < hash.getSize(); i++) {
    totalSize += hash.getBucketSize(i);
  }
  cout << totalSize << " " << keys.size() - 20 << endl;
}

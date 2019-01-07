#include "clist.h"
#include "rwlist.h"
#include <vector>
#include <set>
#include <iostream>
#include <algorithm>
#include <stdlib.h>
using namespace std;

int main(int argc, char** argv) {
  set<int> map;
  srand(time(NULL));
  for (int i = 0; i < 40000; i++) {
    map.insert(rand() % 50000 * 2);
  }
  rwlist list(0);

  //create random list
  vector<int> keys(map.begin(), map.end());
  for (int i = 0; i < keys.size(); i++) {
    int idx = rand() % keys.size();
    int temp = keys[i];
    keys[i] = keys[idx];
    keys[idx] = temp;
  }
  //insert values
  for (int i = 0; i < keys.size(); i++) {
    list.insert(keys[i]);
  }

  //check insertions
  sort(keys.begin(), keys.end());
  for (int i = 0; i < keys.size(); i++) {
    if (list.lookup(keys[i]) == false) {
      cout << "Did not find value: " << keys[i] << endl;
    }
  }

  //check sorted insertions
  for (int i = 0; i < keys.size(); i++) {
    if (list.getElement(i) != keys[i]) {
      cout << "Did not get the right element: " << keys[i] << endl;
    }
  }

  //test remove on three important sections
  for (int i = 0; i < 10; i++) {
    list.remove(keys[i]);
  }
  for (int i = keys.size() - 10; i < keys.size(); i++) {
    list.remove(keys[i]);
  }

  for (int i = 10; i < keys.size() - 10; i++) {
    if (list.lookup(keys[i]) == false) {
      cout << "Did not remove properly" << endl;
    }
  }

  for (int i = 0; i < 10; i++) {
    if (list.lookup(keys[i]) == true) {
      cout << "Did not remove properly" << endl;
    }
  }
  for (int i = keys.size() - 10; i < keys.size(); i++) {
    if (list.lookup(keys[i]) == true) {
      cout << "Did not remove properly" << endl;
    }
  }

  for (int i = 0; i < keys.size() - 20; i++) {
    if (keys[i + 10] != list.getElement(i)) {
      cout << "Did not remove properly" << endl;
    }
  }

  //ensure size is the same
  cout << list.getSize() << " " << keys.size() - 20 << endl;
}

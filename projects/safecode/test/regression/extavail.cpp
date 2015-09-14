//
// RUN: clang -S -emit-llvm -O2 -fmemsafety %s -o - 2>&1 | grep TargetList 2>&1 | grep internal | grep -v _ZNSaIcEC1Ev | wc -l | grep "      0"
//
// XFAIL: linux
//
// This is a test case for PR#16672:
// http://llvm.org/bugs/show_bug.cgi?id=16672
//
// We check to see that functions marked as externally available are not
// included in the list of CFI targets
//

#include <string>
#include <map>
#include <iostream>

using namespace std;
int main(int argc,char** argv){
    map<string,int> themap;
    string test(argv[0]);

    themap["key1"]=1;
    cout << test << endl;

    return 0;
}


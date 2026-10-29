#include "../src/LiveLease.h"
#include <assert.h>
int main(){LiveLease lease;assert(!lease.valid(0));lease.accept(1800000000000ULL,1800000090,100);assert(lease.valid(100));assert(lease.valid(85000));assert(!lease.valid(89000));lease.invalidate();assert(!lease.valid(100));assert(lease.required);lease.accept(1800000000000ULL,1800000090,UINT32_MAX-1000);assert(lease.valid(100));assert(!lease.valid(90000));lease.accept(1800000000000ULL,1700000000,0);assert(!lease.valid(0));}

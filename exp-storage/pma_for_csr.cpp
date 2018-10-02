#include <assert.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include "pma_for_csr.h"

using namespace std;


int pma_for_csr::log2(int n) {
	int lg2 = 0;
	while (n > 1) {
		n /= 2;
		++lg2;
	}
	return lg2;
}


pma_for_csr::pma_for_csr(int **reflistadr, int capacity = 2, double gub = 0.7) : nElems(0) {
	
	assert(capacity > 1);
	assert(1 << log2(capacity) == capacity);

	this->init_vars(capacity);
	this->impl = (ent **)malloc(capacity*sizeof(ent*));
	this->present = (int *)malloc(capacity*sizeof(int));
	memset(this->present, 0, capacity*sizeof(int));
	this->global_upperbound = gub;
	this->tmp = NULL;
	this->wrapper = reflistadr;
}

void pma_for_csr::init_vars(int capacity) {
	this->chunk_size = 1 << log2(log2(capacity) * 2);
	assert(this->chunk_size == (1 << log2(this->chunk_size)));
	this->nChunks = capacity / this->chunk_size;
	this->nLevels = log2(this->nChunks);
	this->lgn = log2(capacity);
	this->capacity = capacity;
	// printf("init_vars:: capacity: %d, nElems: %d, chunk_size: %d, nChunks: %d\n", capacity, nElems, chunk_size, nChunks);
}

double pma_for_csr::upper_threshold_at(int level) const {
	assert(level <= this->nLevels);
	return level == 0 ? this->global_upperbound : this->global_upperbound - ((this->global_upperbound - 0.5) * level) / (double)this->nLevels;
}

int pma_for_csr::left_interval_boundary(int i, int interval_size) {
	assert(interval_size == (1 << log2(interval_size)));
	assert(i < (int)this->capacity);
	int q = i / interval_size;
	int boundary = q * interval_size;
	// printf("left_interval_boundary(%d, %d) = %d\n", i, interval_size, boundary);
	return boundary;
}

void pma_for_csr::resize(int capacity) {


	assert(capacity > this->capacity);
	assert(1 << log2(capacity) == capacity);

	ent **tmpi = (ent **)malloc(capacity*sizeof(ent*));
	int *tmpp = (int *)malloc(capacity*sizeof(int));
	memset(tmpp, 0, capacity*sizeof(int));

	printf("new cap %d\n", capacity);

	double d = (double)capacity / this->nElems;
	int ctr = 0;
	for (int i = 0; i < (int)this->capacity; ++i) {
		if (this->present[i]) {
			int idx = d*(ctr++);
			tmpp[idx] = 1;
			tmpi[idx] = this->impl[i];
			this->impl[i] = NULL;
		}
	}

	free(this->impl);
	free(this->present);
	this->impl = tmpi;
	this->present = tmpp;
	this->init_vars(capacity);

}

void pma_for_csr::get_interval_stats(int left, int level, bool &in_limit, int &sz, int nos) {
	double t = upper_threshold_at(level);
	int w = (1 << level) * this->chunk_size;
	sz = 0;
	for (int i = left; i < left + w; ++i) { 
		sz += this->present[i] ? 1 : 0;
		// if(this->present[i])
		// 	printf("element present at index : %d\n", i);
	}
	double q = (double)(sz + nos) / double(w);
	in_limit = q <= t;
	printf("%f , %f \n", q, t);
}

int pma_for_csr::val_in_chunk(int l, int v) {
	int i;
	for (i = l; i < l + chunk_size; ++i) 
		if (this->present[i]) 
			if (this->impl[i]->val >= v) 
				return i;
	return i;
}

int pma_for_csr::lower_bound(int v) {
	int i;
	if (this->nElems == 0) {
		i = this->capacity;
	} 
	else {
		int l = 0, r = this->nChunks;
		int m;
		while (l != r) {
			printf("l : %d, r : %d \n", l, r);
			m = l + (r-l)/2;
			int left = left_interval_boundary(m * chunk_size, chunk_size);
			int pos = val_in_chunk(left, v);
			if (pos == left + chunk_size) 
				l = m + 1;
			else 
				r = m;
		}
		i = l * chunk_size;
	}
	return i;
}

void pma_for_csr::insert_in_window(int rl, int rh, int l, int v) {

	printf("to insert %d, \n", v);
	printf("rl : %d, rh : %d, l : %d\n", rl, rh, l);


	this->tmp = (ent **)malloc(this->chunk_size*sizeof(ent *));
	int ntmp = 0;
	bool inserted = false;

	for (int i = l; i < l + this->chunk_size; ++i) {
		if (this->present[i]) {
			if(i >= rl && i < rh && !inserted && this->impl[i]->val >= v) {
				this->tmp[ntmp++] = new ent(v, 0);
				i--;
				inserted = true;
				continue;
			}
			this->present[i] = 0;
			this->tmp[ntmp++] = this->impl[i];
			this->impl[i] = NULL;
		}

	}

	if(!inserted)
		this->tmp[ntmp++] = new ent(v, 0);

	for(int i=0;i<ntmp;i++) {
		this->present[l+i] = 1;
		this->impl[l+i] = this->tmp[i];
		this->tmp[i] = NULL;
	}

	// double m = (double)this->chunk_size / (double)ntmp;
	// assert(m >= 1.0);
	// for (int i = 0; i < ntmp; ++i) {
	// 	int k = i * m + l;
	// 	assert(k < l + this->chunk_size);
	// 	this->present[k] = 1;
	// 	this->impl[k] = tmp[i];
	// }
	free(this->tmp);
	++this->nElems;
}

void pma_for_csr::rebalance_interval(int left, int level) {
	// printf("rebalance_interval(%d, %d)\n", left, level);
	int w = (1 << level) * this->chunk_size;
	this->tmp = (ent **)malloc(sizeof(ent*)*w);
	int ntmp = 0;
	for (int i = left; i < left + w; ++i) {
		if (this->present[i]) {
			this->tmp[ntmp++] = this->impl[i];
			this->impl[i] = NULL;
			this->present[i] = 0;
		}
	}
	double m = (double)(1<<level)*chunk_size / (double)ntmp;
	assert(m >= 1.0);
	for (int i = 0; i < ntmp; ++i) {
		int k = i * m + left;
		assert(k < left + w);
		this->present[k] = 1;
		this->impl[k] = this->tmp[i];
		this->tmp[i] = NULL;
	}
	free(this->tmp);
	// this->print();
}

void pma_for_csr::insert(int rl, int v) {

	printf("general case of insertion\n");

	// rl will be the backref, so we increase the value by 1
	rl = rl + 1;

	int i = -1;
	//we need to find the next backprop (or the end of impl)
	int rh = rl;
	for( ;rh < this->capacity; rh++) {
		if(this->present[rh]) {
			if(this->impl[rh]->typ == 1) {
				break;
			}
			if(this->impl[rh]->typ == 0) {
				if(i == -1 &&this->impl[rh]->val >= v)
					i = rh;
			}
		}
	}
	// we have gotten the range in which we can insert
	// if v is the largest element in the range
	if(i == -1)
		i = rh;
	
	assert(i >= rl);
	assert(i <= rh);
	
	// if no range high is found, rh = capacity. if i == rh in this place, it is a problem.
	if(i == this->capacity)
		i--;

	int w = chunk_size;
	int level = 0;
	int l = this->left_interval_boundary(i, w);

	int sz;
	bool in_limit;
	get_interval_stats(l, level, in_limit, sz, 1);

	if(in_limit)
		this->insert_in_window(rl, rh, l, v);
	else {
		while (!in_limit) {
			w *= 2;
			level += 1;
			if (level > this->nLevels) {
				printf("resizing ... \n");
				this->resize(2 * this->capacity);
				this->insert(rl - 1, v);
				return;
			}

			l = this->left_interval_boundary(i, w);
			get_interval_stats(l, level, in_limit, sz, 1);
			// printf("level: %d, this->nLevels: %d, in_limit: %d, sz: %d\n", level, this->nLevels, in_limit, sz);
		}
		this->rebalance_interval(l, level);
		this->insert(rl - 1, v);
	}

	this->print();
}


void pma_for_csr::firstInsert(int insertLoc, int cas, int v, int nref) {

	printf("--> first insert\n");

	ent *s = new ent(nref, 1);
	int rl;

	switch(cas) {
		case 1 :
			printf("case 1, insertLoc : %d, nodelistref : %d\n", insertLoc, nref);
			this->present[insertLoc] = 1;
			this->impl[insertLoc] = s;
			++this->nElems;
			rl = insertLoc;
			this->print();
			break;
		
		case 3 :
			// rl = this->insert_s(s, l);
			break;
		
		default :
			printf("case 2, insertLoc : %d, nodelistref : %d\n", insertLoc, nref);
			if(this->present[this->capacity - 1])
				this->resize(2*this->capacity);

			this->present[this->capacity - 1] = 1;
			this->impl[this->capacity - 1] = s;
			++this->nElems;
			rl = this->capacity - 1;
			this->print();
			break;

	} 

	wrapper[0][nref] = rl;
	this->insert(rl, v);

} 


void pma_for_csr::print() {
	for (int i = 0; i < (int)this->capacity; ++i) {
		if(this->present[i])
			printf("%d\t", this->impl[i]->val);
		else 
			printf("-1\t");
	}
	printf("\n");

	for (int i = 0; i < (int)this->capacity; ++i) {
		if(this->present[i])
			printf("%d\t", this->impl[i]->typ);
		else 
			printf("-1\t");
	}
	printf("\n");
}

// int main() {

// 	pma_for_csr obj(2,0.9);

// 	for(int i=0;i<50000;i++) {
// 		int r = rand()%50000000;
// 		printf("Inserting %d ...\n", r);
// 		obj.insert(r);
// 		obj.print();
// 		printf("----------------------------------------------------\n");
// 	}

// 	// printf("%d", rand());

// 	// obj.insert(80);
// 	// obj.print();
//  //    obj.insert(50);
// 	// obj.print();
//  //    obj.insert(70);
// 	// obj.print();
//  //    obj.insert(90);
// 	// obj.print();
//  //    obj.insert(65);
// 	// obj.print();
// 	// obj.print();
//  //    obj.insert(85);
// 	// obj.print();
//  //    obj.insert(10);
// 	// obj.print();
//  //    obj.insert(21);
// 	// obj.print();
//  //    obj.insert(22);
// 	// obj.print();
//  //    obj.insert(20);
// 	// obj.print();
//  //    obj.insert(24);
// 	// obj.print();
//  //    obj.insert(15);
// 	// obj.print();
//  //    obj.insert(17);
// 	// obj.print();
//  //    obj.insert(23);
// 	// obj.print();
    

// }

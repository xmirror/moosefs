/*
   Copyright Jakub Kruszona-Zawadzki, Core Technology Sp. z o.o.

   This file is part of MooseFS.

   READ THIS BEFORE INSTALLING THE SOFTWARE. BY INSTALLING,
   ACTIVATING OR USING THE SOFTWARE, YOU ARE AGREEING TO BE BOUND BY
   THE TERMS AND CONDITIONS OF MooseFS LICENSE AGREEMENT FOR
   VERSION 1.7 AND HIGHER IN A SEPARATE FILE. THIS SOFTWARE IS LICENSED AS
   THE PROPRIETARY SOFTWARE. YOU NOT ACQUIRE
   ANY OWNERSHIP RIGHT, TITLE OR INTEREST IN OR TO ANY INTELLECTUAL
   PROPERTY OR OTHER PROPRIETARY RIGHTS.
 */

#ifndef _MEDIAN_H_
#define _MEDIAN_H_

#include <inttypes.h>

static inline double median_find(double *array, uint32_t n) {
	int32_t l,r,c,m,i,j;
	double tmp;

	l = 0;
	r = n-1;
	m = (l+r)/2;
	for (;;) {
		if (r<=l) {
			return array[m];
		}
		if ((r-l)==1) {
			if (array[l] > array[r]) {
				tmp = array[l];
				array[l] = array[r];
				array[r] = tmp;
			}
			return array[m];
		}
		c = (l+r)/2;
		if (array[c] > array[r]) {
			tmp = array[c];
			array[c] = array[r];
			array[r] = tmp;
		}
		if (array[l] > array[r]) {
			tmp = array[l];
			array[l] = array[r];
			array[r] = tmp;
		}
		if (array[c] > array[l]) {
			tmp = array[c];
			array[c] = array[l];
			array[l] = tmp;
		}
		i = l+1;
		j = r;
		tmp = array[c];
		array[c] = array[i];
		array[i] = tmp;
		for (;;) {
			do {
				i++;
			} while (array[l] > array[i]);
			do {
				j--;
			} while (array[j] > array[l]);
			if (j<i) {
				break;
			}
			tmp = array[i];
			array[j] = array[i];
			array[j] = tmp;
		}
		tmp = array[l];
		array[l] = array[j];
		array[j] = tmp;

		if (j<=m) {
			l = i;
		}
		if (j>=m) {
			r = j-1;
		}
	}
}

#endif

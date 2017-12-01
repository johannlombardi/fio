#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memcpy.h"
#include "rand.h"
#include "../fio_time.h"
#include "../gettime.h"
#include "../fio.h"

#define BUF_SIZE	32 * 1024 * 1024ULL

#define NR_ITERS	64

struct memcpy_test {
	const char *name;
	void *src;
	void *dst;
	size_t size;
};

static struct memcpy_test tests[] = {
	{
		.name		= "8 bytes",
		.size		= 8,
	},
	{
		.name		= "16 bytes",
		.size		= 16,
	},
	{
		.name		= "96 bytes",
		.size		= 96,
	},
	{
		.name		= "128 bytes",
		.size		= 128,
	},
	{
		.name		= "256 bytes",
		.size		= 256,
	},
	{
		.name		= "512 bytes",
		.size		= 512,
	},
	{
		.name		= "2048 bytes",
		.size		= 2048,
	},
	{
		.name		= "8192 bytes",
		.size		= 8192,
	},
	{
		.name		= "131072 bytes",
		.size		= 131072,
	},
	{
		.name		= "262144 bytes",
		.size		= 262144,
	},
	{
		.name		= "524288 bytes",
		.size		= 524288,
	},
	{
		.name		= NULL,
	},
};

struct memcpy_type {
	const char *name;
	unsigned int mask;
	void (*fn)(struct memcpy_type *, struct memcpy_test *);
};

enum {
	T_MEMCPY	= 1U << 0,
	T_MEMMOVE	= 1U << 1,
	T_SIMPLE	= 1U << 2,
};

#define do_test(t, test, fn)	do {					\
	size_t left, this;						\
	void *src, *dst;						\
	int i;								\
									\
	for (i = 0; i < NR_ITERS; i++) {				\
		left = BUF_SIZE;					\
		src = test->src;					\
		dst = test->dst;					\
		while (left) {						\
			this = test->size;				\
			if (this > left)				\
				this = left;				\
			(fn)(dst, src, this);				\
			left -= this;					\
			src += this;					\
			dst += this;					\
		}							\
	}								\
} while (0)

static void t_memcpy(struct memcpy_type *t, struct memcpy_test *test)
{
	do_test(t, test, memcpy);
}

static void t_memmove(struct memcpy_type *t, struct memcpy_test *test)
{
	do_test(t, test, memmove);
}

static void simple_memcpy(void *dst, void const *src, size_t len)
{
 	char *d = dst;
	const char *s = src;

	while (len--)
		*d++ = *s++;
}

static void t_simple(struct memcpy_type *t, struct memcpy_test *test)
{
	do_test(t, test, simple_memcpy);
}

static struct memcpy_type t[] = {
	{
		.name = "memcpy",
		.mask = T_MEMCPY,
		.fn = t_memcpy,
	},
	{
		.name = "memmove",
		.mask = T_MEMMOVE,
		.fn = t_memmove,
	},
	{
		.name = "simple",
		.mask = T_SIMPLE,
		.fn = t_simple,
	},

	{
		.name = NULL,
	},
};

static unsigned int get_test_mask(const char *type)
{
	char *ostr, *str = strdup(type);
	unsigned int mask;
	char *name;
	int i;

	ostr = str;
	mask = 0;
	while ((name = strsep(&str, ",")) != NULL) {
		for (i = 0; t[i].name; i++) {
			if (!strcmp(t[i].name, name)) {
				mask |= t[i].mask;
				break;
			}
		}
	}

	free(ostr);
	return mask;
}

static int list_types(void)
{
	int i;

	for (i = 0; t[i].name; i++)
		printf("%s\n", t[i].name);

	return 1;
}

static int setup_tests(void)
{
	struct memcpy_test *test;
	struct frand_state state;
	void *src, *dst;
	int i;

	if (posix_memalign(&src, page_size, BUF_SIZE))
		return 1;
	if (posix_memalign(&dst, page_size, BUF_SIZE))
		return 1;

	init_rand_seed(&state, 0x8989, 0);
	fill_random_buf(&state, src, BUF_SIZE);

	for (i = 0; tests[i].name; i++) {
		test = &tests[i];
		test->src = src;
		test->dst = dst;
	}

	return 0;
}

int fio_memcpy_test(const char *type)
{
	unsigned int test_mask = 0;
	int j, i;

	if (!type)
		test_mask = ~0U;
	else if (!strcmp(type, "help") || !strcmp(type, "list"))
		return list_types();
	else
		test_mask = get_test_mask(type);

	if (!test_mask) {
		fprintf(stderr, "fio: unknown hash `%s`. Available:\n", type);
		return list_types();
	}

	if (setup_tests()) {
		fprintf(stderr, "setting up mem regions failed\n");
		return 1;
	}

	for (i = 0; t[i].name; i++) {
		struct timespec ts;
		double mb_sec;
		uint64_t usec;

		if (!(t[i].mask & test_mask))
			continue;

		/*
		 * For first run, make sure CPUs are spun up and that
		 * we've touched the data.
		 */
		usec_spin(100000);
		t[i].fn(&t[i], &tests[0]);

		printf("%s\n", t[i].name);

		for (j = 0; tests[j].name; j++) {
			fio_gettime(&ts, NULL);
			t[i].fn(&t[i], &tests[j]);
			usec = utime_since_now(&ts);

			if (usec) {
				unsigned long long mb = NR_ITERS * BUF_SIZE;

				mb_sec = (double) mb / (double) usec;
				mb_sec /= (1.024 * 1.024);
				printf("\t%s:\t%8.2f MiB/sec\n", tests[j].name, mb_sec);
			} else
				printf("\t%s:inf MiB/sec\n", tests[j].name);
		}
	}

	return 0;
}

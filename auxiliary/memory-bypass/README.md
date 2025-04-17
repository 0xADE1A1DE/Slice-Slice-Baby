Building
========

In order to build the kernel module, you will need Clang version 11 or later. You can visit [this website](https://apt.llvm.org/) to install Clang 12 for your distribution, if your distribution does not already provide it.

On Ubuntu you will need the following dependencies:

```
sudo apt install build-essential git
```

In addition, you will need the kernel headers:

```
sudo apt install linux-headers-$(uname -r)
```

Since the kernel module is written in Rust, you will also need [rustup](https://rustup.rs). If you don't have the Rust nightly toolchain installed, install it as follows:

```
rustup install nightly-2021-02-22
```

**Note**: the current version of Rust nightly is broken as of writing (2021-03-15).

Switch the repository to use the Rust nightly toolchain:

```
rustup override set nightly-2021-02-22
```

Since we are using a specific version, we have to add the rust-src component:

```
rustup component add rust-src
```

Building the kernel module:

```
make
```

On Ubuntu you may have to specify that you want to use clang-12 as follows:

```
CC=clang-12 make
```

Loading the kernel module:

```
sudo insmod memkit.ko
```

Unload the kernel module:

```
sudo rmmod memkit.ko
```

Using the API
=============

The memkit kernel module uses the `kretprobe` API from the Linux kernel to override the return value of the `devmem_is_allowed()` function to always return true. This way, rather than only having access to the first MiB of your physical memory through `/dev/mem`, you should be able to access all physical memory. You can simply open the `/dev/mem` using the `open()` system call, and then use the `pread()` and `pwrite()` system calls to read from or write to physical memory, where the offset is the physical address.

The memkit module also install its own device called `/dev/memkit` with custom ioctls. See the `include/memkit.h` header for the exact defintions to use from C. There are also [Rust bindings](https://git.codentium.com/StephanvanSchaik/rascal).

Below is an example of how you can allocate a page using the `mmap()` system call, and then ask the memkit kernel module to resolve the page table hierarchy and return the corresponding PTE for that virtual address in C:

```
#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memkit.h>

int main()
{
	struct memkit_resolve req = { 0 };
	void *page;
	int fd;

	// Map in a page.
	page = mmap(NULL, 4096, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);

	if (page == MAP_FAILED)
		return -1;

	// Open the memkit device.
	fd = open("/dev/memkit", O_RDONLY | O_CLOEXEC);

	if (fd < 0)
		goto out_map;

	// Get the PTE corresponding with the page in the current address space.
	req.ptr = (uin64_t)page;
	req.pid = 0;

	// Resolve the page table hierarchy to get the PTE.
	err = ioctl(fd, MEMKIT_IOC_READ_PTE, &req);

	if (err < 0)
		goto out_open;

	// Display the PTE.
	printf("PTE: 0x%016llx\n", req.pte);

out_open:
	close(fd);
out_map:
	munmap(page, 4096);
	return 0;
}
```

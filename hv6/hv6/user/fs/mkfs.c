#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fslayout.h"
/* avoid clash with host struct stat */
#define stat xv6_stat
#include "stat.h"

#define SIZE 1000
//#define NINODES 4000
//#define NINODES 3500
#define NINODES 4000

// Disk layout:
// [ boot block | sb block | inode blocks | bit map | data blocks | log ]

// by amadeus chan
// there are 7 logically seperated disks used in Yxv6fs, including log(WAL) disk, block-bitmap disk, inode meta-data disk, inode-bitmap disk, data disk, orphan inode disk, block-pointer(Virtual WAL) disk
// okay, so I have to rewrite this makefs tool
// nbitmap: number of block bitmap blocks
// ninodeblocks: number of inode meta-data blocks
// nlog: number of log blocks
int nbitmap = SIZE / (BSIZE * 8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;   // Number of meta blocks (inode, bitmap, and 2 extra)
int nblocks; // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void balloc(int);
void wsect(uint, void *);
void winode(uint, struct dinode *);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);

// convert to intel byte order
ushort xshort(ushort x)
{
    ushort y;
    uchar *a = (uchar *)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

uint xint(uint x)
{
    uint y;
    uchar *a = (uchar *)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

int main(int argc, char *argv[])
{
    int i, cc, fd;
    uint rootino, inum, off;
    struct dirent de;
    char buf[BSIZE];
    struct dinode din;

    static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

    if (argc < 2) {
        fprintf(stderr, "Usage: mkfs fs.img files...\n");
        exit(1);
    }

    assert((BSIZE % sizeof(struct dinode)) == 0);
    assert((BSIZE % sizeof(struct dirent)) == 0);

    fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fsfd < 0) {
        perror(argv[1]);
        exit(1);
    }

    nmeta = 2 + ninodeblocks + nbitmap;
    nblocks = SIZE - nlog - nmeta;

    sb.size = xint(SIZE);
    sb.nblocks = xint(nblocks); // so whole disk is size sectors
    sb.ninodes = xint(NINODES);
    sb.nlog = xint(nlog);

    printf("fs layout: size: %d, nblocks: %d, ninodes: %d, nlog: %d, IPB: %d\n", SIZE, nblocks, NINODES, nlog, IPB);

    printf("nmeta %d (boot, super, inode blocks %u, bitmap blocks %u) "
           "blocks %d log %u total %d\n",
           nmeta, ninodeblocks, nbitmap, nblocks, nlog, SIZE);

    freeblock = nmeta; // the first free block that we can allocate

    for (i = 0; i < SIZE; i++)
        wsect(i, zeroes);

    memset(buf, 0, sizeof(buf));
    memmove(buf, &sb, sizeof(sb));
    wsect(1, buf);

    rootino = ialloc(T_DIR);
    assert(rootino == ROOTINO);

    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, ".");
    iappend(rootino, &de, sizeof(de));

    bzero(&de, sizeof(de));
    de.inum = xshort(rootino);
    strcpy(de.name, "..");
    iappend(rootino, &de, sizeof(de));

    for (i = 2; i < argc; i++) {
        const char *path;

        if ((fd = open(argv[i], 0)) < 0) {
            perror(argv[i]);
            exit(1);
        }

        /* skip / */
        path = strrchr(argv[i], '/');
        if (path)
            ++path;
        else
            path = argv[i];

        // Skip leading _ in name when writing to file system.
        // The binaries are named _rm, _cat, etc. to keep the
        // build operating system from trying to execute them
        // in place of system binaries like rm and cat.
        if (path[0] == '_')
            ++path;

        inum = ialloc(T_FILE);

        bzero(&de, sizeof(de));
        de.inum = xshort(inum);
        strncpy(de.name, path, DIRSIZ);
        iappend(rootino, &de, sizeof(de));

        while ((cc = read(fd, buf, sizeof(buf))) > 0)
            iappend(inum, buf, cc);

        close(fd);
    }

    // fix size of root inode dir
    rinode(rootino, &din);
    off = xint(din.size);
    off = ((off / BSIZE) + 1) * BSIZE;
    din.size = xint(off);
    winode(rootino, &din);

    balloc(freeblock); // allocate all blocks up to freeblock

    exit(0);
}

void wsect(uint sec, void *buf)
{
    if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
        perror("lseek");
        exit(1);
    }
    if (write(fsfd, buf, BSIZE) != BSIZE) {
        perror("write");
        exit(1);
    }
}

void winode(uint inum, struct dinode *ip)
{
    char buf[BSIZE];
    uint bn;
    struct dinode *dip;

    bn = IBLOCK(inum);
    rsect(bn, buf);
    dip = ((struct dinode *)buf) + (inum % IPB);
    *dip = *ip;
    printf("winode %d bn %d\n", inum, bn);
    wsect(bn, buf);
}

void rinode(uint inum, struct dinode *ip)
{
    char buf[BSIZE];
    uint bn;
    struct dinode *dip;

    bn = IBLOCK(inum);
    rsect(bn, buf);
    dip = ((struct dinode *)buf) + (inum % IPB);
    *ip = *dip;
}

void rsect(uint sec, void *buf)
{
    if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
        perror("lseek");
        exit(1);
    }
    if (read(fsfd, buf, BSIZE) != BSIZE) {
        perror("read");
        exit(1);
    }
}

uint ialloc(ushort type)
{
    uint inum = freeinode++;
    struct dinode din;

    bzero(&din, sizeof(din));
    din.type = xshort(type);
    din.nlink = xshort(1);
    din.size = xint(0);
    winode(inum, &din);
    return inum;
}

void balloc(int used)
{
    uchar buf[BSIZE];
    int i;

    printf("balloc: first %d blocks have been allocated\n", used);
    assert(used < BSIZE * 8);
    bzero(buf, BSIZE);
    for (i = 0; i < used; i++) {
        buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
    }
    printf("balloc: write bitmap block at sector %d\n", ninodeblocks + 2);
    wsect(ninodeblocks + 2, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void iappend(uint inum, void *xp, int n)
{
    char *p = (char *)xp;
    uint fbn, off, n1;
    struct dinode din;
    char buf[BSIZE];
    uint indirect[NINDIRECT], dindirect[NINDIRECT];
    uint x;

    rinode(inum, &din);

    off = xint(din.size);
    while (n > 0) {
        fbn = off / BSIZE;
        assert(fbn < MAXFILE);
        if (fbn < NDIRECT) {
            if (xint(din.addrs[fbn]) == 0) {
                din.addrs[fbn] = xint(freeblock++);
            }
            x = xint(din.addrs[fbn]);
        } else if (fbn - NDIRECT < NINDIRECT) {
            if (xint(din.addrs[NDIRECT]) == 0) {
                // printf("allocate indirect block\n");
                din.addrs[NDIRECT] = xint(freeblock++);
            }
            // printf("read indirect block\n");
            rsect(xint(din.addrs[NDIRECT]), (char *)indirect);
            if (indirect[fbn - NDIRECT] == 0) {
                indirect[fbn - NDIRECT] = xint(freeblock++);
                wsect(xint(din.addrs[NDIRECT]), (char *)indirect);
            }
            x = xint(indirect[fbn - NDIRECT]);
        } else {
            fbn -= NINDIRECT + NDIRECT;
            if (xint(din.addrs[NDIRECT + 1] == 0)) {
                din.addrs[NDIRECT + 1] = xint(freeblock++);
            }
            rsect(xint(din.addrs[NDIRECT + 1]), (char *)indirect);
            if (indirect[fbn / NINDIRECT] == 0) {
                indirect[fbn / NINDIRECT] = xint(freeblock++);
                wsect(xint(din.addrs[NDIRECT + 1]), (char *)indirect);
            }
            rsect(xint(indirect[fbn / NINDIRECT]), (char *)dindirect);
            if (dindirect[fbn % NINDIRECT] == 0) {
                dindirect[fbn % NINDIRECT] = xint(freeblock++);
                wsect(xint(indirect[fbn / NINDIRECT]), (char *)dindirect);
            }
            x = dindirect[fbn % NINDIRECT];
            fbn += NINDIRECT + NDIRECT;
        }
        n1 = min(n, (fbn + 1) * BSIZE - off);
        rsect(x, buf);
        bcopy(p, buf + off - (fbn * BSIZE), n1);
        wsect(x, buf);
        n -= n1;
        off += n1;
        p += n1;
    }
    din.size = xint(off);
    winode(inum, &din);
}

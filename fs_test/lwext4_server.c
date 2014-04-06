
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <time.h>

#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#endif


#include <ext4_filedev.h>
#include <io_raw.h>

#include <ext4.h>

static int winsock_init(void);
static void winsock_fini(void);
static char* entry_to_str(uint8_t type);

#define MAX_FILES   64
#define MAX_DIRS    64

#define MAX_RW_BUFFER   (1024 * 1024)
#define RW_BUFFER_PATERN    ('x')


/**@brief   Default connection port*/
static int connection_port = 1234;

/**@brief   Default filesystem filename.*/
static char *ext4_fname = "ext2";

/**@brief   Verbose mode*/
static int verbose = 0;

/**@brief   Winpart mode*/
static int winpart = 0;

/**@brief   Blockdev handle*/
static struct ext4_blockdev *bd;

static char read_buffer[MAX_RW_BUFFER];
static char write_buffer[MAX_RW_BUFFER];


static const char *usage = "                                    \n\
Welcome in lwext4_server.                                       \n\
Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)  \n\
Usage:                                                          \n\
    --image    (-i) - ext2/3/4 image file                       \n\
    --port     (-p) - server port                               \n\
    --verbose  (-v) - verbose mode                              \n\
    --winpart  (-w) - windows_partition mode                    \n\
\n";



/**@brief   Open file instance descriptor.*/
struct lwext4_files {
    char        name[255];
    ext4_file   fd;
};

/**@brief   Open directory instance descriptor.*/
struct lwext4_dirs {
    char        name[255];
    ext4_dir    fd;
};

/**@brief   Library call opcode.*/
struct lwext4_op_codes {
    char *func;
};

/**@brief   Library call wraper.*/
struct lwext4_call {
    int (*lwext4_call)(char *p);
};

/**@brief  */
static struct lwext4_files file_tab[MAX_FILES];

/**@brief  */
static struct lwext4_dirs  dir_tab[MAX_DIRS];

/**@brief  */
static struct lwext4_op_codes op_codes[] = {
        "device_register",
        "mount",
        "umount",
        "mount_point_stats",
        "cache_write_back",
        "fremove",
        "fopen",
        "fclose",
        "fread",
        "fwrite",
        "fseek",
        "ftell",
        "fsize",
        "dir_rm",
        "dir_mk",
        "dir_open",
        "dir_close",
        "dir_entry_get",

        "multi_fcreate",
        "multi_fwrite",
        "multi_fread",
        "multi_fremove",
        "multi_dcreate",
        "multi_dremove",
        "stats_save",
        "stats_check",
};

int _device_register(char *p);
int _mount(char *p);
int _umount(char *p);
int _mount_point_stats(char *p);
int _cache_write_back(char *p);
int _fremove(char *p);
int _fopen(char *p);
int _fclose(char *p);
int _fread(char *p);
int _fwrite(char *p);
int _fseek(char *p);
int _ftell(char *p);
int _fsize(char *p);
int _dir_rm(char *p);
int _dir_mk(char *p);
int _dir_open(char *p);
int _dir_close(char *p);
int _dir_close(char *p);
int _dir_entry_get(char *p);

int _multi_fcreate(char *p);
int _multi_fwrite(char *p);
int _multi_fread(char *p);
int _multi_fremove(char *p);
int _multi_dcreate(char *p);
int _multi_dremove(char *p);
int _stats_save(char *p);
int _stats_check(char *p);

/**@brief  */
static struct lwext4_call op_call[] = {
        _device_register,  /*PARAMS(3):   0 cache_mode dev_name   */
        _mount,            /*PARAMS(2):   dev_name mount_point    */
        _umount,           /*PARAMS(1):   mount_point             */
        _mount_point_stats,/*PARAMS(2):   mount_point, 0          */
        _cache_write_back, /*PARAMS(2):   mount_point, en         */
        _fremove,          /*PARAMS(1):   path                    */
        _fopen,            /*PARAMS(2):   fid path flags          */
        _fclose,           /*PARAMS(1):   fid                     */
        _fread,            /*PARAMS(4):   fid 0 len 0             */
        _fwrite,           /*PARAMS(4):   fid 0 len 0             */
        _fseek,            /*PARAMS(2):   fid off origin          */
        _ftell,            /*PARAMS(2):   fid exp                 */
        _fsize,            /*PARAMS(2):   fid exp                 */
        _dir_rm,           /*PARAMS(1):   path                    */
        _dir_mk,           /*PARAMS(1):   path                    */
        _dir_open,         /*PARAMS(2):   did, path               */
        _dir_close,        /*PARAMS(1):   did                     */
        _dir_entry_get,    /*PARAMS(2):   did, exp                */

        _multi_fcreate,    /*PARAMS(3):   path prefix cnt         */
        _multi_fwrite,     /*PARAMS(4):   path prefix cnt size    */
        _multi_fread,      /*PARAMS(4):   path prefix cnt size    */
        _multi_fremove,    /*PARAMS(2):   path prefix cnt         */
        _multi_dcreate,    /*PARAMS(3):   path prefix cnt         */
        _multi_dremove,    /*PARAMS(2):   path prefix             */
        _stats_save,       /*PARAMS(1):   path                    */
        _stats_check,      /*PARAMS(1):   path                    */
};

static clock_t get_ms(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}

/**@brief  */
static int exec_op_code(char *opcode)
{
    int i;
    int r = -1;

    for (i = 0; i < sizeof(op_codes) / sizeof(op_codes[0]); ++i) {

        if(strncmp(op_codes[i].func, opcode, strlen(op_codes[i].func)))
            continue;

        if(opcode[strlen(op_codes[i].func)] != ' ')
            continue;

        printf("%s\n", opcode);
        opcode += strlen(op_codes[i].func);
        /*Call*/

        clock_t t = get_ms();
        r = op_call[i].lwext4_call(opcode);

        printf("rc: %d, time: %ums\n", r, (unsigned int)(get_ms() - t));

        break;
    }

    return r;
}


static int server_open(void)
{
    int fd = 0;
    struct sockaddr_in serv_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));

    if(winsock_init() < 0) {
        printf("winsock_init() error\n");
        exit(-1);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        printf("socket() error: %s\n", strerror(errno));
        exit(-1);
    }

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(int))) {
        printf("setsockopt() error: %s\n", strerror(errno));
        exit(-1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(connection_port);

    if(bind(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))){
        printf("bind() error: %s\n", strerror(errno));
        exit(-1);
    }

    if(listen(fd, 1)){
        printf("listen() error: %s\n", strerror(errno));
        exit(-1);
    }

    return fd;
}

static bool parse_opt(int argc, char **argv)
{
    int option_index = 0;
    int c;

    static struct option long_options[] = {
            {"image",   required_argument, 0, 'i'},
            {"port",    required_argument, 0, 'p'},
            {"verbose", required_argument, 0, 'v'},
            {"winpart", required_argument, 0, 'w'},
            {0, 0, 0, 0}
    };

    while(-1 != (c = getopt_long (argc, argv, "i:p:v:w:", long_options, &option_index))) {

        switch(c){
        case 'i':
            ext4_fname = optarg;
            break;
        case 'p':
            connection_port = atoi(optarg);
            break;
        case 'v':
            verbose = atoi(optarg);
            break;
        case 'w':
            winpart = atoi(optarg);
            break;
        default:
            printf("%s", usage);
            return false;

        }
    }
    return true;
}

int main(int argc, char *argv[])
{
    int n;
    int listenfd;
    int connfd;
    char op_code[128];

    if(!parse_opt(argc, argv))
        return -1;

    listenfd = server_open();

    printf("lwext4_server: listening on port: %d\n", connection_port);

    memset(write_buffer, RW_BUFFER_PATERN, MAX_RW_BUFFER);
    while(1)
    {
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

        n = recv(connfd, op_code, sizeof(op_code), 0);

        if(n < 0) {
            printf("recv() error: %s fd = %d\n", strerror(errno), connfd);
            break;
        }

        op_code[n] = 0;

        int r = exec_op_code(op_code);

        n = send(connfd, (void *)&r, sizeof(r), 0);
        if(n < 0) {
            printf("send() error: %s fd = %d\n", strerror(errno), connfd);
            break;
        }

        close(connfd);
    }

    winsock_fini();
    return 0;
}


int _device_register(char *p)
{
    int dev;
    int cache_mode;
    char dev_name[32];

    if(sscanf(p, "%d %d %s", &dev, &cache_mode, dev_name) != 3){
        printf("Param list error\n");
        return -1;
    }

#ifdef WIN32
    if(winpart){
        ext4_io_raw_filename(ext4_fname);
        bd = ext4_io_raw_dev_get();

    }
    else
#endif
    {
        ext4_filedev_filename(ext4_fname);
        bd = ext4_filedev_get();
    }
    return ext4_device_register(bd, 0, dev_name);
}

int _mount(char *p)
{
    char dev_name[32];
    char mount_point[32];
    int rc;

    if(sscanf(p, "%s %s", dev_name, mount_point) != 2){
        printf("Param list error\n");
        return -1;
    }

    return ext4_mount(dev_name, mount_point);
}

int _umount(char *p)
{
    char mount_point[32];

    if(sscanf(p, "%s", mount_point) != 1){
        printf("Param list error\n");
        return -1;
    }

    return ext4_umount(mount_point);
}

int _mount_point_stats(char *p)
{
    char mount_point[32];
    int d;
    int rc;
    struct ext4_mount_stats stats;

    if(sscanf(p, "%s %d", mount_point, &d) != 2){
        printf("Param list error\n");
        return -1;
    }

    rc = ext4_mount_point_stats(mount_point, &stats);

    if(rc != EOK)
        return;

    if(verbose){
        printf("\tinodes_count = %d\n", stats.inodes_count);
        printf("\tfree_inodes_count = %d\n", stats.free_inodes_count);
        printf("\tblocks_count = %llu\n", stats.blocks_count);
        printf("\tfree_blocks_count = %llu\n", stats.free_blocks_count);

        printf("\tblock_size = %d\n", stats.block_size);
        printf("\tblock_group_count = %d\n", stats.block_group_count);
        printf("\tblocks_per_group = %d\n", stats.blocks_per_group);
        printf("\tinodes_per_group = %d\n", stats.inodes_per_group);

        printf("\tvolume_name = %s\n", stats.volume_name);
    }

    return rc;
}

int _cache_write_back(char *p)
{
    char mount_point[32];
    int en;

    if(sscanf(p, "%s %d", mount_point, &en) != 2){
        printf("Param list error\n");
        return -1;
    }

    return ext4_cache_write_back(mount_point, en);
}


int _fremove(char *p)
{
    char path[255];

    if(sscanf(p, "%s", path) != 1){
        printf("Param list error\n");
        return -1;
    }

    return ext4_fremove(path);
}

int _fopen(char *p)
{
    int fid = MAX_FILES;
    char path[256];
    char flags[8];
    int rc;

    if(sscanf(p, "%d %s %s", &fid, path, flags) != 3){
        printf("Param list error\n");
        return -1;
    }

    if(!(fid < MAX_FILES)){
        printf("File id too big\n");
        return -1;
    }

    rc = ext4_fopen(&file_tab[fid].fd, path, flags);

    if(rc == EOK)
        strcpy(file_tab[fid].name, path);

    return rc;
}

int _fclose(char *p)
{
    int fid = MAX_FILES;
    int rc;

    if(sscanf(p, "%d", &fid) != 1){
        printf("Param list error\n");
        return -1;
    }

    if(!(fid < MAX_FILES)){
        printf("File id too big\n");
        return -1;
    }

    if(file_tab[fid].name[0] == 0){
        printf("File id empty\n");
        return -1;
    }

    rc = ext4_fclose(&file_tab[fid].fd);

    if(rc == EOK)
        file_tab[fid].name[0] = 0;

    return rc;
}

int _fread(char *p)
{
    int fid = MAX_FILES;
    int len;
    int d;
    int rc;
    int rb;

    if(sscanf(p, "%d %d %d %d", &fid, &d, &len, &d) != 4){
        printf("Param list error\n");
        return -1;
    }

    if(!(fid < MAX_FILES)){
        printf("File id too big\n");
        return -1;
    }

    if(file_tab[fid].name[0] == 0){
        printf("File id empty\n");
        return -1;
    }

    while(len){
        d = len > MAX_RW_BUFFER ? MAX_RW_BUFFER : len;

        memset(read_buffer, 0, MAX_RW_BUFFER);
        rc = ext4_fread(&file_tab[fid].fd, read_buffer, d, &rb);

        if(rc != EOK)
            break;

        if(rb != d){
            printf("Read count error\n");
            return -1;
        }

        if(memcmp(read_buffer, write_buffer, d)){
            printf("Read compare error\n");
            return -1;
        }

        len -= d;
    }

    return rc;
}

int _fwrite(char *p)
{
    int fid = MAX_FILES;
    int len;
    int d;
    int rc;
    int wb;

    if(sscanf(p, "%d %d %d %d", &fid, &d, &len, &d) != 4){
        printf("Param list error\n");
        return -1;
    }

    if(!(fid < MAX_FILES)){
        printf("File id too big\n");
        return -1;
    }

    if(file_tab[fid].name[0] == 0){
        printf("File id empty\n");
        return -1;
    }

    while(len){
        d = len > MAX_RW_BUFFER ? MAX_RW_BUFFER : len;
        rc = ext4_fwrite(&file_tab[fid].fd, write_buffer, d, &wb);

        if(rc != EOK)
            break;

        if(wb != d){
            printf("Write count error\n");
            return -1;
        }

        len -= d;
    }

    return rc;
}

int _fseek(char *p)
{
    int fid = MAX_FILES;
    int off;
    int origin;

    if(sscanf(p, "%d %d %d", &fid, &off, &origin) != 3){
        printf("Param list error\n");
        return -1;
    }

    if(!(fid < MAX_FILES)){
        printf("File id too big\n");
        return -1;
    }

    if(file_tab[fid].name[0] == 0){
        printf("File id empty\n");
        return -1;
    }

    return ext4_fseek(&file_tab[fid].fd, off, origin);
}

int _ftell(char *p)
{
    int fid = MAX_FILES;
    uint32_t exp_pos;

    if(sscanf(p, "%d %u", &fid, &exp_pos) != 2){
        printf("Param list error\n");
        return -1;
    }

    if(!(fid < MAX_FILES)){
        printf("File id too big\n");
        return -1;
    }

    if(file_tab[fid].name[0] == 0){
        printf("File id empty\n");
        return -1;
    }


    if(exp_pos != ext4_ftell(&file_tab[fid].fd)){
        printf("Expected filepos error\n");
        return -1;
    }

    return EOK;
}

int _fsize(char *p)
{
    int fid = MAX_FILES;
    uint32_t exp_size;

    if(sscanf(p, "%d %u", &fid, &exp_size) != 2){
        printf("Param list error\n");
        return -1;
    }

    if(!(fid < MAX_FILES)){
        printf("File id too big\n");
        return -1;
    }

    if(file_tab[fid].name[0] == 0){
        printf("File id empty\n");
        return -1;
    }

    if(exp_size != ext4_fsize(&file_tab[fid].fd)){
        printf("Expected filesize error\n");
        return -1;
    }

    return EOK;
}

int _dir_rm(char *p)
{
    char path[255];

    if(sscanf(p, "%s", path) != 1){
        printf("Param list error\n");
        return -1;
    }

    return ext4_dir_rm(path);
}

int _dir_mk(char *p)
{
    char path[255];

    if(sscanf(p, "%s", path) != 1){
        printf("Param list error\n");
        return -1;
    }

    return ext4_dir_mk(path);
}

int _dir_open(char *p)
{
    int did = MAX_DIRS;
    char path[255];
    int rc;

    if(sscanf(p, "%d %s", &did, path) != 2){
        printf("Param list error\n");
        return -1;
    }

    if(!(did < MAX_DIRS)){
        printf("Dir id too big\n");
        return -1;
    }

    rc = ext4_dir_open(&dir_tab[did].fd, path);

    if(rc == EOK)
        strcpy(dir_tab[did].name, path);

    return rc;
}

int _dir_close(char *p)
{
    int did = MAX_DIRS;
    int rc;

    if(sscanf(p, "%d", &did) != 1){
        printf("Param list error\n");
        return -1;
    }

    if(!(did < MAX_DIRS)){
        printf("Dir id too big\n");
        return -1;
    }

    if(dir_tab[did].name[0] == 0){
        printf("Dir id empty\n");
        return -1;
    }

    rc = ext4_dir_close(&dir_tab[did].fd);

    if(rc == EOK)
        dir_tab[did].name[0] = 0;

    return rc;
}

int _dir_entry_get(char *p)
{
    int did = MAX_DIRS;
    int exp;
    char name[256];

    if(sscanf(p, "%d %d", &did, &exp) != 2){
        printf("Param list error\n");
        return -1;
    }

    if(!(did < MAX_DIRS)){
        printf("Dir id too big\n");
        return -1;
    }

    if(dir_tab[did].name[0] == 0){
        printf("Dir id empty\n");
        return -1;
    }


    int idx = 0;
    ext4_direntry *d;

    while(d = ext4_dir_entry_get(&dir_tab[did].fd, idx++)){

        memcpy(name, d->name, d->name_length);
        name[d->name_length] = 0;
        if(verbose){
            printf("\t%s %s\n", entry_to_str(d->inode_type), name);
        }
    }

    idx--;

    if(idx < 2){
        printf("Minumum dir entry error\n");
        return -1;
    }

    if((idx - 2) != exp){
        printf("Expected dir entry error\n");
        return -1;
    }

    return EOK;
}

int _multi_fcreate(char *p)
{
    char path[256];
    char path1[256];
    char prefix[32];
    int cnt;
    int rc;
    int i;
    ext4_file   fd;

    if(sscanf(p, "%s %s %d", path, prefix, &cnt) != 3){
        printf("Param list error\n");
        return -1;
    }

    for (i = 0; i < cnt; ++i) {
        sprintf(path1, "%s%s%d", path, prefix, i);
        rc = ext4_fopen(&fd, path1, "wb+");

        if(rc != EOK)
            break;
    }

    return rc;
}

int _multi_fwrite(char *p)
{
    char path[256];
    char path1[256];
    char prefix[32];
    int cnt;
    int len, ll;
    int rc;
    int i, d, wb;
    ext4_file   fd;

    if(sscanf(p, "%s %s %d %d", path, prefix, &cnt, &ll) != 4){
        printf("Param list error\n");
        return -1;
    }

    for (i = 0; i < cnt; ++i) {
        sprintf(path1, "%s%s%d", path, prefix, i);
        rc = ext4_fopen(&fd, path1, "rb+");

        if(rc != EOK)
            break;

        len = ll;
        while(len){
            d = len > MAX_RW_BUFFER ? MAX_RW_BUFFER : len;
            rc = ext4_fwrite(&fd, write_buffer, d, &wb);

            if(rc != EOK)
                break;

            if(wb != d){
                printf("Write count error\n");
                return -1;
            }

            len -= d;
        }
    }

    return rc;
}

int _multi_fread(char *p)
{
    char path[256];
    char path1[256];
    char prefix[32];
    int cnt;
    int len, ll;
    int rc;
    int i, d, rb;
    ext4_file   fd;

    if(sscanf(p, "%s %s %d %d", path, prefix, &cnt, &ll) != 4){
        printf("Param list error\n");
        return -1;
    }

    for (i = 0; i < cnt; ++i) {
        sprintf(path1, "%s%s%d", path, prefix, i);
        rc = ext4_fopen(&fd, path1, "rb+");

        if(rc != EOK)
            break;

        len = ll;
        while(len){
            d = len > MAX_RW_BUFFER ? MAX_RW_BUFFER : len;

            memset(read_buffer, 0, MAX_RW_BUFFER);
            rc = ext4_fread(&fd, read_buffer, d, &rb);

            if(rc != EOK)
                break;

            if(rb != d){
                printf("Read count error\n");
                return -1;
            }

            if(memcmp(read_buffer, write_buffer, d)){
                printf("Read compare error\n");
                return -1;
            }

            len -= d;
        }
    }

    return rc;
}

int _multi_fremove(char *p)
{
    char path[256];
    char path1[256];
    char prefix[32];
    int  cnt, i, rc;

    if(sscanf(p, "%s %s %d", path, prefix, &cnt) != 3){
        printf("Param list error\n");
        return -1;
    }

    for (i = 0; i < cnt; ++i) {
        sprintf(path1, "%s%s%d", path, prefix, i);
        rc = ext4_fremove(path1);
        if(rc != EOK)
            break;
    }

    return rc;
}

int _multi_dcreate(char *p)
{
    char path[256];
    char path1[256];
    char prefix[32];
    int  cnt, i, rc;

    if(sscanf(p, "%s %s %d", path, prefix, &cnt) != 3){
        printf("Param list error\n");
        return -1;
    }

    for (i = 0; i < cnt; ++i) {
        sprintf(path1, "%s%s%d", path, prefix, i);
        rc = ext4_dir_mk(path1);
        if(rc != EOK)
            break;
    }

    return rc;
}

int _multi_dremove(char *p)
{
    char path[256];
    char path1[256];
    char prefix[32];
    int  cnt, i, rc;

    if(sscanf(p, "%s %s %d", path, prefix, &cnt) != 3){
        printf("Param list error\n");
        return -1;
    }

    for (i = 0; i < cnt; ++i) {
        sprintf(path1, "%s%s%d", path, prefix, i);
        rc = ext4_dir_rm(path1);
        if(rc != EOK)
            break;
    }

    return rc;
}

struct ext4_mount_stats  saved_stats;

int _stats_save(char *p)
{
    char path[256];

    if(sscanf(p, "%s", path) != 1){
        printf("Param list error\n");
        return -1;
    }

    return ext4_mount_point_stats(path, &saved_stats);
}

int _stats_check(char *p)
{
    char path[256];
    int rc;

    struct ext4_mount_stats  actual_stats;

    if(sscanf(p, "%s", path) != 1){
        printf("Param list error\n");
        return -1;
    }

    rc = ext4_mount_point_stats(path, &actual_stats);

    if(rc != EOK)
        return rc;

    if(memcmp(&saved_stats, &actual_stats, sizeof(struct ext4_mount_stats))){
        if(verbose){
            printf("\tMount point stats error:\n");
            printf("\tsaved_stats:\n");
            printf("\tinodes_count = %d\n", saved_stats.inodes_count);
            printf("\tfree_inodes_count = %d\n", saved_stats.free_inodes_count);
            printf("\tblocks_count = %llu\n", saved_stats.blocks_count);
            printf("\tfree_blocks_count = %llu\n", saved_stats.free_blocks_count);
            printf("\tblock_size = %d\n", saved_stats.block_size);
            printf("\tblock_group_count = %d\n", saved_stats.block_group_count);
            printf("\tblocks_per_group = %d\n", saved_stats.blocks_per_group);
            printf("\tinodes_per_group = %d\n", saved_stats.inodes_per_group);
            printf("\tvolume_name = %s\n", saved_stats.volume_name);
            printf("\tactual_stats:\n");
            printf("\tinodes_count = %d\n", actual_stats.inodes_count);
            printf("\tfree_inodes_count = %d\n", actual_stats.free_inodes_count);
            printf("\tblocks_count = %llu\n", actual_stats.blocks_count);
            printf("\tfree_blocks_count = %llu\n", actual_stats.free_blocks_count);
            printf("\tblock_size = %d\n", actual_stats.block_size);
            printf("\tblock_group_count = %d\n", actual_stats.block_group_count);
            printf("\tblocks_per_group = %d\n", actual_stats.blocks_per_group);
            printf("\tinodes_per_group = %d\n", actual_stats.inodes_per_group);
            printf("\tvolume_name = %s\n", actual_stats.volume_name);
        }
        return -1;
    }


   return rc;
}


static char* entry_to_str(uint8_t type)
{
    switch(type){
    case EXT4_DIRENTRY_UNKNOWN:
        return "[UNK] ";
    case EXT4_DIRENTRY_REG_FILE:
        return "[FIL] ";
    case EXT4_DIRENTRY_DIR:
        return "[DIR] ";
    case EXT4_DIRENTRY_CHRDEV:
        return "[CHA] ";
    case EXT4_DIRENTRY_BLKDEV:
        return "[BLK] ";
    case EXT4_DIRENTRY_FIFO:
        return "[FIF] ";
    case EXT4_DIRENTRY_SOCK:
        return "[SOC] ";
    case EXT4_DIRENTRY_SYMLINK:
        return "[SYM] ";
    default:
        break;
    }
    return "[???]";
}

static int winsock_init(void)
{
#if WIN32
    int rc;
    static WSADATA wsaData;
    rc = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (rc != 0) {
        return -1;
    }
#endif
    return 0;
}

static void winsock_fini(void)
{
#if WIN32
    WSACleanup();
#endif
}


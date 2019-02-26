#include "maildir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

DIR *open_or_create_directory(const char *path, int *error)
{
    DIR *d = opendir(path);
    if (d != NULL)
    {   
        *error = 0;
        return d;
    }

    if (errno == ENOENT)
    {
        if (mkdir(path, 0777) == 0)
        {
            d = opendir(path);
            *error = (d == NULL) ? errno : 0; 

            return d;
        }
    }

    *error = errno;
    return NULL;
}

char *open_subdirectory(const char *root_dir, const char *subdir_name, int *error)
{
    int root_len = strlen(root_dir);
    int path_len = root_len + strlen(subdir_name) + 1;

    char path_buf[path_len];
    memset(path_buf, 0, path_len);

    strcpy(path_buf, root_dir);
    strcat(path_buf, subdir_name);
    DIR *d = open_or_create_directory(path_buf, error);
    
    if (!(*error))
    {
        closedir(d);
        return strdup(path_buf);
    }
    
    return NULL;
}

void maildir_close(maildir_t *maildir)
{
    if (maildir->root_path)
        free(maildir->root_path);
    if (maildir->cur_path)
        free(maildir->cur_path);
    if (maildir->relay_path)
        free(maildir->relay_path);
    if (maildir->tmp_path)
        free(maildir->tmp_path);
    *maildir = (maildir_t){0};
}

maildir_t maildir_open(const char *root_path, int *error)
{
    maildir_t mail = {0};

    mail.root_path = open_subdirectory(root_path, "", error);
    if (*error != 0)
    {
        maildir_close(&mail);
        return mail;
    }
    
    mail.cur_path = open_subdirectory(root_path, "/cur/", error);
    if (*error != 0)
    {
        maildir_close(&mail);
        return mail;
    }

    mail.tmp_path = open_subdirectory(root_path, "/tmp/", error);
    if (*error != 0)
    {
        maildir_close(&mail);
        return mail;
    }

    mail.relay_path = open_subdirectory(root_path, "/relay/", error);
    if (*error != 0)
    {
        maildir_close(&mail);
        return mail;
    }

    return mail;
}

int maildir_create_in_tmp(maildir_t *m, char *name)
{
    int dirfd = open(m->tmp_path, O_RDONLY, 0644);
    int fd = openat(dirfd, name, O_CREAT | O_WRONLY, 0644);
    close(dirfd);
    return fd;
}

void maildir_move_to_cur(maildir_t *m, char *filename)
{
    int tmpdirfd = open(m->tmp_path, O_RDONLY, 0644);
    int curdirfd = open(m->cur_path, O_RDONLY, 0644);
    
    renameat(tmpdirfd, filename, curdirfd, filename);
    
    close(tmpdirfd);
    close(curdirfd);
}

void maildir_move_to_relay(maildir_t *m, char *filename)
{
    int tmpdirfd = open(m->tmp_path, O_RDONLY, 0644);
    int relaydirfd = open(m->relay_path, O_RDONLY, 0644);
    
    renameat(tmpdirfd, filename, relaydirfd, filename);
    
    close(tmpdirfd);
    close(relaydirfd);
}

void generate_filename(char *buf, int len,
        struct timespec time, int random_number, int pid, int worker_id, int deliveries, char *hostname)

{
    snprintf(buf, len, "%ld.R%dM%dP%dT%dQ%d.%s", (long int)time.tv_sec,
             random_number, (int)(time.tv_nsec / 1000), pid, worker_id, deliveries,
             hostname); 
}
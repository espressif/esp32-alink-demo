/*
 * Copyright (c) 2014-2015 Alibaba Group. All rights reserved.
 *
 * Alibaba Group retains all right, title and interest (including all
 * intellectual property rights) in and to this computer program, which is
 * protected by applicable intellectual property laws.  Unless you have
 * obtained a separate written license from Alibaba Group., you are not
 * authorized to utilize all or a part of this computer program for any
 * purpose (including reproduction, distribution, modification, and
 * compilation into object code), and you must immediately destroy or
 * return to Alibaba Group all copies of this computer program.  If you
 * are licensed by Alibaba Group, your rights to utilize this computer
 * program are limited by the terms of that license.  To obtain a license,
 * please contact Alibaba Group.
 *
 * This computer program contains trade secrets owned by Alibaba Group.
 * and, unless unauthorized by Alibaba Group in writing, you agree to
 * maintain the confidentiality of this computer program and related
 * information and to not disclose this computer program and related
 * information to any other person or entity.
 *
 * THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
 * Alibaba Group EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, TITLE, AND NONINFRINGEMENT.
 */
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include "os/platform/platform.h"


#define handle_error_en(en, msg) \
        do { printf(msg); printf("%d\n", en); goto do_exit; } while (0)

void platform_printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	fflush(stdout);
}

/************************ memory manage ************************/


void *platform_malloc(uint32_t size)
{
	return malloc(size);
}


void platform_free(void *ptr)
{
	free(ptr);
}



/************************ mutex manage ************************/

void *platform_mutex_init(void)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	if (NULL == mutex)
	{
		return NULL;
	}

	if (0 != pthread_mutex_init(mutex, NULL))
	{
        free(mutex);
        return NULL;
	}

	return mutex;
}


void platform_mutex_lock(void *mutex)
{
	pthread_mutex_lock((pthread_mutex_t *)mutex);
}


void platform_mutex_unlock(void *mutex)
{
	pthread_mutex_unlock((pthread_mutex_t *)mutex);
}


void platform_mutex_destroy(void *mutex)
{
	pthread_mutex_destroy((pthread_mutex_t *)mutex);
	free(mutex);
}


/************************ semaphore manage ************************/

void *platform_semaphore_init(void)
{
	sem_t *sem = (sem_t *)malloc(sizeof(sem_t));
	if (NULL == sem)
	{
		return NULL;
	}

	if (0 != sem_init(sem, 0, 0))
	{
        free(sem);
        return NULL;
	}

	return sem;
}




int platform_semaphore_wait(_IN_ void *sem, _IN_ uint32_t timeout_ms)
{
	if (PLATFORM_WAIT_INFINITE == timeout_ms)
	{
		sem_wait(sem);
		return 0;
	}
	else
	{
		struct timespec ts;
		int s;

		if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
		{
			return -1;
		}

		s = 0;
		ts.tv_nsec += (timeout_ms % 1000) * 1000000;
		if (ts.tv_nsec >= 10000000)
		{
			ts.tv_nsec -= 10000000;
			s = 1;
		}

		ts.tv_sec += timeout_ms / 1000 + s;

		//ret = sem_timedwait((sem_t *)sem, &ts);

		/* Restart if interrupted by handler */
		do
		{
	        if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
	        {
	            return -1;
	        }

	        s = 0;
	        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
	        if (ts.tv_nsec >= 10000000)
	        {
	            ts.tv_nsec -= 10000000;
	            s = 1;
	        }

	        ts.tv_sec += timeout_ms / 1000 + s;

		} while (((s = sem_timedwait(sem, &ts)) != 0) && errno == EINTR);

		return (s == 0) ? 0 : -1;
	}
}


void platform_semaphore_post(void *sem)
{
	sem_post((sem_t *)sem);
}


void platform_semaphore_destroy(void *sem)
{
	sem_destroy((sem_t *)sem);
	free(sem);
}
//
//typedef struct
//{
//	unsigned int counter;
//	pthread_mutex_t mutex;
//	pthread_cond_t cond;
//}platform_sem_t;
//
//void *platform_semaphore_init(void)
//{
//	int ret;
//
//	platform_sem_t *sem = (platform_sem_t *)malloc(sizeof(platform_sem_t));
//	if (NULL == sem)
//	{
//		handle_error_en(0, "memory allocate fail.");
//		return NULL;
//	}
//
//	memset(sem, 0, sizeof(platform_sem_t));
//
//	ret = pthread_mutex_init(&(sem->mutex), NULL);
//	if (0 != ret)
//	{
//		handle_error_en(0, "pthread_mutex_init error.");
//	}
//
//	pthread_cond_init(&(sem->cond), NULL);
//	if (0 != ret)
//	{
//		handle_error_en(0, "pthread_cond_init error.");
//	}
//
//	return sem;
//
//do_exit:
//
//	//if (NULL != sem->cond)
//	if (1)
//	{
//		pthread_cond_destroy(&(sem->cond));
//	}
//
//	//if (NULL != sem->mutex)
//	if (1)
//	{
//		pthread_mutex_destroy(&(sem->mutex));
//	}
//
//	if (NULL != sem)
//	{
//		free(sem);
//	}
//	return NULL;
//}
//
//
//
//
//int platform_semaphore_wait(_IN_ void *sem, _IN_ uint32_t timeout_ms)
//{
//	int ret;
//	platform_sem_t *psem = (platform_sem_t *)sem;
//
//	pthread_mutex_lock(&psem->mutex);
//
//	if (psem->counter > 0)
//	{
//		--psem->counter;
//		pthread_mutex_unlock(&psem->mutex);
//		return 0;
//	}
//
//	if (PLATFORM_WAIT_INFINITE == timeout_ms)
//	{
//		 pthread_cond_wait(&psem->cond, &psem->mutex);
//		 --psem->counter;
//		 pthread_mutex_unlock(&psem->mutex);
//		 return 0;
//	}
//	else
//	{
//		struct timespec ts;
//		int s;
//
//		if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
//		{
//			return -1;
//		}
//
//		s = 0;
//		ts.tv_nsec += (timeout_ms % 1000) * 1000000;
//		if (ts.tv_nsec >= 10000000)
//		{
//			ts.tv_nsec -= 10000000;
//			s = 1;
//		}
//
//		ts.tv_sec += timeout_ms / 1000 + s;
//
//		ret = pthread_cond_timedwait(&psem->cond, &psem->mutex, &ts);
//		if (0 == ret)
//		{
//			--psem->counter;
//			ret = 0;
//		}
//		else
//		{
//			ret = -1;
//		}
//
//		pthread_mutex_unlock(&psem->mutex);
//		return ret;
//	}
//}
//
//
//void platform_semaphore_post(void *sem)
//{
//	platform_sem_t *psem = (platform_sem_t *)sem;
//
//	pthread_mutex_lock(&psem->mutex);
//
//	++psem->counter;
//	pthread_cond_signal(&psem->cond);
//
//	pthread_mutex_unlock(&psem->mutex);
//}
//
//
//void platform_semaphore_destroy(void *sem)
//{
//	platform_sem_t *psem = (platform_sem_t *)sem;
//
//	pthread_cond_destroy(&(psem->cond));
//	pthread_mutex_destroy(&(psem->mutex));
//	free(sem);
//}


/****************************************************************************/


void platform_msleep(_IN_ uint32_t ms)
{
	usleep( 1000 * ms );
}


uint32_t platform_get_time_ms(void)
{
	struct timeval tv = { 0 };
	uint32_t time_ms;

	gettimeofday(&tv, NULL);

	time_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	return time_ms;
}


int platform_thread_get_stack_size(_IN_ const char *thread_name)
{
    if (0 == strcmp(thread_name, "alink_main_thread")) 
    {
        printf("get alink_main_thread\n");
        return 0xc00;
    }
    else if (0 == strcmp(thread_name, "wsf_worker_thread"))
    {
        printf("get wsf_worker_thread\n");
        return 0x2100;
    }
    else if (0 == strcmp(thread_name, "firmware_upgrade_pthread"))
    {
        printf("get firmware_upgrade_pthread\n");
        return 0xc00;
    }
    else if (0 == strcmp(thread_name, "send_worker"))
    {
        printf("get send_worker\n");
        return 0x800;
    }
    else if (0 == strcmp(thread_name, "callback_thread"))
    {
        printf("get callback_thread\n");
        return 0x800;     
    }
    else 
    {
        printf("get othrer thread\n");
        return 0x800;
    }

    assert(0);
}
int platform_thread_create(
		_OUT_ void **thread,
		_IN_ const char *name,
		_IN_ void *(*start_routine) (void *),
		_IN_ void *arg,
		_IN_ void *stack,
		_IN_ uint32_t stack_size,
		_OUT_ int *stack_used)
{
	int s, ret = -1;
	pthread_attr_t attr;

	s = pthread_attr_init(&attr);
	if (s != 0)
	{
		handle_error_en(s, "pthread_attr_init error.");
	}

	if (stack_size > 0)
	{
		//NOTE: multiple 10 times, in linux platform.
		stack_size *= 100;

		s = pthread_attr_setstacksize(&attr, stack_size);
		if (0 != s)
		{
			handle_error_en(s, "pthread_attr_setstacksize error.");
		}
	}

	pthread_create((pthread_t *)thread, &attr, start_routine, arg);
	if (0 != s)
	{
		handle_error_en(s, "pthread_create error");
	}

	ret = 0;
    *stack_used = 0;

do_exit:
	s = pthread_attr_destroy(&attr);
	if (0 != s)
	{
		printf("pthread_attr_destroy error. \n");
	}

	return ret;
}



void platform_thread_exit(void *thread)
{
	pthread_exit(0);
}


char *platform_get_chipid(char chipid[PLATFORM_CID_LEN])
{
    return strncpy(chipid, "rtl8188eu 12345678", PLATFORM_CID_LEN);
}

char *platform_get_os_version(char os_ver[PLATFORM_OS_VERSION_LEN])
{
    return strncpy(os_ver, "ubuntu 12.04 LTS", PLATFORM_OS_VERSION_LEN);
}

int platform_config_write(const char *buffer, int length)
{
    FILE *fp;
    size_t written_len;

    if (!buffer || length <= 0)
        return -1;

    fp = fopen("alinkconfig.db", "w");
    if (!fp)
        return -1;

    written_len = fwrite(buffer, 1, length, fp);

    fclose(fp);

    return ((written_len != length) ? -1: 0);
}

int platform_config_read(char *buffer, int length)
{
    FILE *fp;
    size_t read_len;

    if (!buffer || length <= 0)
        return -1;

    fp = fopen("alinkconfig.db", "r");
    if (!fp)
        return -1;

    read_len = fread(buffer, 1, length, fp);
    fclose(fp);

    return ((read_len != length)? -1: 0);
}


int platform_sys_net_is_ready(void)
{
    return 1;
}

void platform_sys_reboot(void)
{
    //TODO
}

char *platform_get_module_name(char name_str[PLATFORM_MODULE_NAME_LEN])
{
    strncpy(name_str, "linux", STR_SHORT_LEN);
    name_str[PLATFORM_MODULE_NAME_LEN-1]='\0';
    return name_str;
}

static FILE *fp;
#define otafilename "alinkota.bin"
void platform_firmware_upgrade_start(void)
{
    fp = fopen(otafilename, "w");
    assert(fp);
    return;
}
int platform_firmware_upgrade_write(_IN_ char *buffer, _IN_ uint32_t length)
{
    unsigned int written_len = 0;
    written_len = fwrite(buffer, 1, length, fp);
    
    if (written_len != length)
        return -1;
    return 0;
}

int platform_firmware_upgrade_finish(void)
{
    if(fp != NULL) {
        fclose(fp);
    }

    return 0;
}


/*
 * Copyright (c) 2004, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/**
 * \file
 * Linked list library implementation.
 *
 * \author Adam Dunkels <adam@sics.se>
 *
 */

/**
 * \addtogroup list
 * @{
 */
#include "contiki.h"
#include "lib/list.h"

#include <string.h>
/*---------------------------------------------------------------------------*/
struct list {
  struct list *next;
};
/*---------------------------------------------------------------------------*/
void *
list_tail(const_list_t list)
{
  struct list *l;

  if(*list == NULL) {
    return NULL;
  }

  for(l = *list; l->next != NULL; l = l->next);

  return l;
}
/*---------------------------------------------------------------------------*/
void
list_add(list_t list, void *item)
{
  struct list *l;

  /* Make sure not to add the same element twice */
  list_remove(list, item);
	
  if(item != NULL){
  	((struct list *)item)->next = NULL;

  	l = list_tail(list);

  	if(l == NULL) {
    	*list = item;
  	} else {
    	l->next = item;
  	}
  }
}
/*---------------------------------------------------------------------------*/
void
list_push(list_t list, void *item)
{
  /* Make sure not to add the same element twice */
  list_remove(list, item);
	
  if(item != NULL){
  ((struct list *)item)->next = *list;
  *list = item;
  }
}
/*---------------------------------------------------------------------------*/
void *
list_chop(list_t list)
{
  struct list *l, *r;

  if(*list == NULL) {
    return NULL;
  }
  if(((struct list *)*list)->next == NULL) {
    l = *list;
    *list = NULL;
    return l;
  }

  for(l = *list; l->next->next != NULL; l = l->next);

  r = l->next;
  l->next = NULL;

  return r;
}
/*---------------------------------------------------------------------------*/
void *
list_pop(list_t list)
{
  struct list *l;
  l = *list;
  if(*list != NULL) {
    *list = ((struct list *)*list)->next;
  }

  return l;
}
/*---------------------------------------------------------------------------*/
void
list_remove(list_t list, const void *item)
{
  struct list *l, *r;

  if(*list == NULL) {
    return;
  }

  r = NULL;
  for(l = *list; l != NULL; l = l->next) {
    if(l == item) {
      if(r == NULL) {
        /* First on list */
        *list = l->next;
      } else {
        /* Not first on list */
        r->next = l->next;
      }
      l->next = NULL;
      return;
    }
    r = l;
  }
}
/*---------------------------------------------------------------------------*/
int
list_length(const_list_t list)
{
  struct list *l;
  int n = 0;

  for(l = *list; l != NULL; l = l->next) {
    ++n;
  }

  return n;
}
/*---------------------------------------------------------------------------*/
void
list_insert(list_t list, void *previtem, void *newitem)
{
  if(previtem == NULL) {
    list_push(list, newitem);
  } else {
		  if(newitem != NULL){
    		list_remove(list, newitem);
    		((struct list *)newitem)->next = ((struct list *)previtem)->next;
    		((struct list *)previtem)->next = newitem;
		}
  }
}
/*---------------------------------------------------------------------------*/
bool
list_contains(const_list_t list, const void *item)
{
  struct list *l;
  for(l = *list; l != NULL; l = l->next) {
    if(item == l) {
    	return true;
    }
  }
  return false;
}
/*---------------------------------------------------------------------------*/
/** @} */

// -*- C++ -*-

/*

  Heap Layers: An Extensible Memory Allocation Infrastructure
  
  Copyright (C) 2000-2003 by Emery Berger
  http://www.cs.umass.edu/~emery
  emery@cs.umass.edu
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef _SLLIST_H_
#define _SLLIST_H_

#include <assert.h>

/**
 * @class SLList
 * @brief A "memory neutral" singly-linked list.
 * @author Emery Berger
 */

namespace HL {

  class SLList {
  public:
    
    inline SLList (void) {
      clear();
    }

    class Entry;
  
    /// Clear the list.
    inline void clear (void) {
      head.next = NULL;
    }

    /// Get the head of the list.
    inline Entry * get (void) {
      const Entry * e = head.next;
      if (e == NULL) {
	return NULL;
      }
      head.next = e->next;
      if (head.next==(void *)0x666){
          cout << "whoops! something went wrong here " << e << " " << getpid() << endl;
      }
      else{
          //cout << "g: " << e << " " << getpid() << endl;
      }
      return (Entry *) e;
    }

  private:

    /**
     * @brief Remove one item from the list.
     * @warning This method aborts the program if called.
     */
    inline void remove (Entry *) {
      abort();
    }

      bool __search(Entry * e){
          Entry * i = head.next;
          while(i){
              if (i==e){
                  return true;
              }
              else{
                  i=i->next;
              }
          }
          return false;
      }

  public:

    /// Inserts an entry into the head of the list.
    inline void insert (Entry * e) {
        if (e->next!=(void *)0x666){
            //cout << "i: " << e << " " << getpid() << " " << e->next << " i: " << *((int *)0x90a8024) << endl;
        }
        else{
            //cout << "i: " << e << " " << getpid() << " " << e->next << " " << *((int *)((char *)e + 16)) << " i: " << *((int *)0x90a8024) << endl;
        }
        if (e->next!=(void *)0x666 && __search(e)){
            cout << "whoops!: " << e << endl;
            //assert(false);
        }
        e->next = head.next;
        head.next = e;
    }

      inline void checkpoint(){
          head_backup = head;
          //cout << "sllist: checkpoint " << head.next << " " << head_backup.next << endl;
      }

      void restore(){
          //cout << "sllist: restore " << head.next << " " << head_backup.next << endl;
          head=head_backup;
          if (head.next==(void *)0x666){
              //cout << "ummmm! something went wrong here" << endl;
          }

      }
      
    /// An entry in the list.
    class Entry {
    public:
      inline Entry (void)
	: next (NULL)
      {}
      //  private:
      //    Entry * prev;
    public:
      Entry * next;
    };

  private:

    /// The head of the list.
    Entry head;

      Entry head_backup;
  };

}

#endif

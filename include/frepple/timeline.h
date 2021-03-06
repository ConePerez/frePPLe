/***************************************************************************
 *                                                                         *
 * Copyright (C) 2007-2012 by Johan De Taeye, frePPLe bvba                 *
 *                                                                         *
 * This library is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU Affero General Public License as published   *
 * by the Free Software Foundation; either version 3 of the License, or    *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This library is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
 * GNU Affero General Public License for more details.                     *
 *                                                                         *
 * You should have received a copy of the GNU Affero General Public        *
 * License along with this program.                                        *
 * If not, see <http://www.gnu.org/licenses/>.                             *
 *                                                                         *
 ***************************************************************************/

#ifndef TIMELINE
#define TIMELINE

#ifndef DOXYGEN
#include <cmath>
#endif

namespace frepple
{
namespace utils
{

/** @brief This class implements a "sorted list" data structure, sorting
  * "events" based on a date.
  *
  * The data structure has slow insert scalability: O(n)<br>
  * Moving data around in the structure is efficient though: O(1)<br>
  * The class leverages the STL library and also follows its api.<br>
  * The class used to instantiate a timeline must support the
  * "bool operator < (TYPE)".
  *
  * Note that the events store the quantity but NOT the date. We pick up
  * the date from the template type. The reasoning for this choice is that
  * the quantity requires more computation than the date and is worthwhile
  * caching. The date field can be read efficiently from the parent type.
  */
template <class type> class TimeLine
{
    friend class Event;
  public:
    class iterator;
    class const_iterator;
    /** @brief Base class for nodes in the timeline. */
    class Event : public NonCopyable
    {
        friend class TimeLine<type>;
        friend class const_iterator;
        friend class iterator;
      protected:
        Date dt;
        double oh;
        double cum_prod;
        Event* next;
        Event* prev;
        Event() : oh(0), cum_prod(0), next(NULL), prev(NULL) {};

      public:
        virtual ~Event() {};
        virtual double getQuantity() const {return 0.0;}

        /** Return the current onhand value. */
        double getOnhand() const {return oh;}

        /** Return the total produced quantity till the current date. */
        double getCumulativeProduced() const {return cum_prod;}

        /** Return the total consumed quantity till the current date. */
        double getCumulativeConsumed() const {return cum_prod - oh;}

        /** Return the date of the event. */
        const Date& getDate() const {return dt;}

        /** Return a pointer to the owning timeline. */
        virtual TimeLine<type>* getTimeLine() const {return NULL;}

        /** This functions returns the mimimum boundary valid at the time of
          * this event. */
        virtual double getMin(bool inclusive = true) const
        {
          assert(this->getTimeLine());
          EventMinQuantity *m = this->getTimeLine()->lastMin;
          if (inclusive)
            while(m && getDate() < m->getDate()) m = m->prevMin;
          else
            while(m && getDate() <= m->getDate()) m = m->prevMin;
          return m ? m->newMin : 0.0;
        }

        /** This functions returns the maximum boundary valid at the time of
          * this event. */
        virtual double getMax(bool inclusive = true) const
        {
          assert(this->getTimeLine());
          EventMaxQuantity *m = this->getTimeLine()->lastMax;
          if (inclusive)
            while(m && getDate() < m->getDate()) m = m->prevMax;
          else
            while(m && getDate() <= m->getDate()) m = m->prevMax;
          return m ? m->newMax : 0.0;
        }

        virtual unsigned short getType() const = 0;

        /** First criterion is date: earlier dates come first.<br>
          * Second criterion is the size: big events come first.<br>
          * As a third tie-breaking criterion, we use a pointer comparison.<br>
          * This garantuees us a fixed and unambiguous ordering.<br>
          * As a side effect, this makes sure that producers come before
          * consumers. This feature is required to avoid zero-time
          * material shortages.
          */
        bool operator < (const Event& fl2) const
        {
          assert (&fl2);
          if (getDate() != fl2.getDate())
            return getDate() < fl2.getDate();
          else if (fabs(getQuantity() - fl2.getQuantity()) > ROUNDING_ERROR)
            return getQuantity() > fl2.getQuantity();
          else
            return this < &fl2;
        }
    };

    /** @brief A timeline event representing a change of the current value. */
    class EventChangeOnhand : public Event
    {
        friend class TimeLine<type>;
      private:
        double quantity;
      public:
        double getQuantity() const {return quantity;}
        EventChangeOnhand(double qty = 0.0) : quantity(qty) {}
        virtual unsigned short getType() const {return 1;}
    };

    /** @brief A timeline event representing a change of the minimum target. */
    class EventMinQuantity : public Event
    {
        friend class TimeLine<type>;
        friend class Event;
      private:
        double newMin;
      protected:
        EventMinQuantity *prevMin;
      public:
        EventMinQuantity(Date d, double f=0.0) : newMin(f), prevMin(NULL)
        {this->dt = d;}
        void setMin(double f) {newMin = f;}
        virtual double getMin(bool inclusive = true) const
        {
          if (inclusive) return newMin;
          else return prevMin ? prevMin->newMin : 0.0;
        }
        virtual unsigned short getType() const {return 3;}
    };

    /** @brief A timeline event representing a change of the maximum target. */
    class EventMaxQuantity : public Event
    {
        friend class Event;
        friend class TimeLine<type>;
      private:
        double newMax;
      protected:
        EventMaxQuantity *prevMax;
      public:
        EventMaxQuantity(Date d, double f=0.0) : newMax(f), prevMax(NULL)
        {this->dt = d;}
        void setMax(double f) {newMax = f;}
        virtual double getMax(bool inclusive = true) const
        {
          if (inclusive) return newMax;
          else return prevMax ? prevMax->newMax : 0.0;
        }
        virtual unsigned short getType() const {return 4;}
    };

    /** @brief This is bi-directional iterator through the timeline. */
    class const_iterator
    {
      protected:
        const Event* cur;
      public:
        const_iterator() : cur(NULL) {}
        const_iterator(const Event* e) : cur(e) {};
        const_iterator(const iterator& c) : cur(c.cur) {}
        const Event& operator*() const {return *cur;}
        const Event* operator->() const {return cur;}
        const_iterator& operator++() {if (cur) cur = cur->next; return *this;}
        const_iterator operator++(int)
        {const_iterator tmp = *this; ++*this; return tmp;}
        const_iterator& operator--() {if (cur) cur = cur->prev; return *this;}
        const_iterator operator--(int)
        {const_iterator tmp = *this; --*this; return tmp;}
        bool operator==(const const_iterator& x) const {return cur == x.cur;}
        bool operator!=(const const_iterator& x) const {return cur != x.cur;}
    };

    /** @brief This is bi-directional iterator through the timeline. */
    class iterator : public const_iterator
    {
      public:
        iterator() {}
        iterator(Event* e) : const_iterator(e) {};
        Event& operator*() const {return *const_cast<Event*>(this->cur);}
        Event* operator->() const {return const_cast<Event*>(this->cur);}
        iterator& operator++() {if (this->cur) this->cur = this->cur->next; return *this;}
        iterator operator++(int) {iterator tmp = *this; ++*this; return tmp;}
        iterator& operator--() {if (this->cur) this->cur = this->cur->prev; return *this;}
        iterator operator--(int) {iterator tmp = *this; --*this; return tmp;}
        bool operator==(const iterator& x) const {return this->cur == x.cur;}
        bool operator!=(const iterator& x) const {return this->cur != x.cur;}
    };

    TimeLine() : first(NULL), last(NULL), lastMax(NULL), lastMin(NULL) {}
    int size() const
    {
      int cnt(0);
      for (Event* p=first; p; p=p->next) ++cnt;
      return cnt;
    }
    iterator begin() {return iterator(first);}
    iterator begin(Event* e) {return iterator(e);}
    iterator rbegin() {return iterator(last);}
    iterator end() {return iterator(NULL);}
    const_iterator begin() const {return const_iterator(first);}
    const_iterator begin(const Event* e) const {return const_iterator(e);}
    const_iterator rbegin() const {return const_iterator(last);}
    const_iterator end() const {return const_iterator(NULL);}
    bool empty() const {return first==NULL;}
    void insert(Event*);
    void insert(EventChangeOnhand* e, double qty, const Date& d)
    {
      e->quantity = qty;
      e->dt = d;
      insert(static_cast<Event*>(e));
    };
    void erase(Event*);
    void update(EventChangeOnhand*, double, const Date&);

    /** This function is used for debugging purposes.<br>
      * It prints a header line, followed by the date, quantity and on_hand
      * of all events in the timeline.
      */
    void inspect(const string& name) const
    {
      logger << "Inspecting  " << this << ": \"" << name << "\":" << endl;
      for (const_iterator oo=begin(); oo!=end(); ++oo)
        logger << "  " << oo->getDate() << "   "
            << oo->getQuantity() << "    " << oo->getOnhand()
            << "    " << oo->getCumulativeProduced() <<  &*oo << endl;
    }

    /** This functions returns the mimimum valid at a certain date. */
    virtual double getMin(Date d, bool inclusive = true) const
    {
      EventMinQuantity *m = this->lastMin;
      if (inclusive)
        while(m && d < m->getDate()) m = m->prevMin;
      else
        while(m && d <= m->getDate()) m = m->prevMin;
      return m ? m->getMin() : 0.0;
    }

    /** This functions returns the mimimum valid at a certain event. */
    virtual double getMin(const Event *e, bool inclusive = true) const
    {
      if (!e) return 0.0;
      EventMinQuantity *m = this->lastMin;
      if (inclusive)
        while(m && e->getDate() < m->getDate()) m = m->prevMin;
      else
        while(m && e->getDate() <= m->getDate()) m = m->prevMin;
      return m ? m->getMin() : 0.0;
    }

    /** This functions returns the maximum valid at a certain date. */
    virtual double getMax(Date d, bool inclusive = true) const
    {
      EventMaxQuantity *m = this->lastMax;
      if (inclusive)
        while(m && d < m->getDate()) m = m->prevMax;
      else
        while(m && d <= m->getDate()) m = m->prevMax;
      return m ? m->getMax() : 0.0;
    }

    /** This functions returns the mimimum valid at a certain eveny. */
    virtual double getMax(const Event *e, bool inclusive = true) const
    {
      if (!e) return 0.0;
      EventMaxQuantity *m = this->lastMax;
      if (inclusive)
        while(m && e->getDate() < m->getDate()) m = m->prevMax;
      else
        while(m && e->getDate() <= m->getDate()) m = m->prevMax;
      return m ? m->getMax() : 0.0;
    }

    /** This functions returns the mimimum event valid at a certain date. */
    virtual EventMinQuantity* getMinEvent(Date d, bool inclusive = true) const
    {
      EventMinQuantity *m = this->lastMin;
      if (inclusive)
        while(m && d < m->getDate()) m = m->prevMin;
      else
        while(m && d <= m->getDate()) m = m->prevMin;
      return m ? m : NULL;
    }

    /** This functions returns the maximum event valid at a certain date. */
    virtual EventMaxQuantity* getMaxEvent(Date d, bool inclusive = true) const
    {
      EventMaxQuantity *m = this->lastMax;
      if (inclusive)
        while(m && d < m->getDate()) m = m->prevMax;
      else
        while(m && d <= m->getDate()) m = m->prevMax;
      return m ? m : NULL;
    }

    /** This function is used to trace the consistency of the data structure. */
    bool check() const;

  private:
    /** A pointer to the first event in the timeline. */
    Event* first;

    /** A pointer to the last event in the timeline. */
    Event* last;

    /** A pointer to the last maximum change. */
    EventMaxQuantity *lastMax;

    /** A pointer to the last minimum change. */
    EventMinQuantity *lastMin;
};


template <class type> void TimeLine<type>::insert (Event* e)
{
  // Loop through all entities till we find the insertion point
  // While searching from the end, update the onhand and cumulative produced
  // quantity of all nodes passed
  iterator i = rbegin();
  double qty = e->getQuantity();
  if (qty > 0)
    for (; i!=end() && *e<*i; --i)
    {
      i->oh += qty;
      i->cum_prod += qty;
    }
  else
    for (; i!=end() && *e<*i; --i)
      i->oh += qty;

  // Insert
  if (i == end())
  {
    // Insert at the head
    if (first)
      first->prev = e;
    else
      // First element
      last = e;
    e->next = first;
    e->prev = NULL;
    first = e;
    e->oh = qty;
    if (qty>0) e->cum_prod = qty;
    else e->cum_prod = 0;
  }
  else
  {
    // Insert in the middle
    e->prev = &*i;
    e->next = i->next;
    if (i->next)
      i->next->prev = e;
    else
      // New last element
      last = e;
    i->next = e;
    e->oh = i->oh + qty;
    if (qty>0) e->cum_prod = i->cum_prod + qty;
    else e->cum_prod = i->cum_prod;
  }

  if (e->getType() == 3)
  {
    // Insert in the list of minima
    EventMinQuantity *m = static_cast<EventMinQuantity*>(e);
    if (!lastMin || m->getDate() >= lastMin->getDate())
    {
      // New last minimum
      m->prevMin = lastMin;
      lastMin = m;
    }
    else
    {
      EventMinQuantity * o = lastMin;
      while (o->prevMin && m->getDate() >= o->prevMin->getDate())
        o = o->prevMin;
      m->prevMin = o->prevMin;
      o->prevMin = m;
    }
  }
  else if (e->getType() == 4)
  {
    // Insert in the list of maxima
    EventMaxQuantity* m = static_cast<EventMaxQuantity*>(e);
    if (!lastMax || m->getDate() >= lastMax->getDate())
    {
      // New last maximum
      m->prevMax = lastMax;
      lastMax = m;
    }
    else
    {
      EventMaxQuantity *o = lastMax;
      while (o->prevMax && m->getDate() >= o->prevMax->getDate())
        o = o->prevMax;
      m->prevMax = o->prevMax;
      o->prevMax = m;
    }
  }

  // Final debugging check
  assert(check());
}


template <class type> void TimeLine<type>::erase(Event* e)
{
  // Update later entries
  double qty = e->getQuantity();
  if (qty>0)
    for (iterator i = begin(e); i!=end(); ++i)
    {
      i->oh -= qty;
      i->cum_prod -= qty;
    }
  else
    for (iterator i = begin(e); i!=end(); ++i)
      i->oh -= qty;

  if (e->prev)
    e->prev->next = e->next;
  else
    // Erasing the head
    first = e->next;

  if (e->next)
    e->next->prev = e->prev;
  else
    // Erasing the tail
    last = e->prev;

  // Clear prev and next pointers
  e->prev = NULL;
  e->next = NULL;

  // Remove the list of minima
  if (e->getType() == 3)
  {
    EventMinQuantity *m = static_cast<EventMinQuantity*>(e);
    if (lastMin == e)
      // New last minimum
      lastMin = m->prevMin;
    else
    {
      EventMinQuantity *o = lastMin;
      while (o->prevMin != e && o) o = o->prevMin;
      if (o) o->prevMin = m->prevMin;
    }
  }

  // Remove the list of maxima
  else if (e->getType() == 4)
  {
    EventMaxQuantity *m = static_cast<EventMaxQuantity*>(e);
    if (lastMax == e)
      // New last maximum
      lastMax = m->prevMax;
    else
    {
      EventMaxQuantity * o = lastMax;
      while (o->prevMax != e && o) o = o->prevMax;
      if (o) o->prevMax = m->prevMax;
    }
  }

  // Final debugging check
  assert(check());
}


template <class type> void TimeLine<type>::update(EventChangeOnhand* e, double newqty, const Date& d)
{
  // Compute the delta quantity
  double delta = e->quantity - newqty;
  double oldqty = e->quantity;

  // Set the new date and quantity. The algorithm below swaps the element with
  // its predecessor or successor till the timeline is properly sorted again.
  e->dt = d;
  e->quantity = newqty;

  // Update the position in the timeline.
  // Remember that the quantity is also used by the '<' operator! Changing the
  // quantity thus can affect the order of elements.
  while ( e->next && !(*e<*e->next) )
  {
    // Move to a later date
    Event *theNext = e->next;
    Event *theNextNext = theNext->next;
    if (e->prev) e->prev->next = theNext;
    theNext->prev = e->prev;
    theNext->next = e;
    e->prev = theNext;
    e->next = theNextNext;
    if (theNextNext)
      theNextNext->prev = e;
    else
      last = e;
    if (first == e) first = theNext;
    e->oh = theNext->oh;
    e->cum_prod = theNext->cum_prod;
    theNext->oh -= oldqty;
    if (oldqty > 0) theNext->cum_prod -= oldqty;
  }
  while ( e->prev && !(*e->prev<*e) )
  {
    // Move to an earlier date
    Event *thePrev = e->prev;
    Event *thePrevPrev = thePrev->prev;
    if (e->next) e->next->prev = thePrev;
    thePrev->next = e->next;
    thePrev->prev = e;
    e->next = thePrev;
    e->prev = thePrevPrev;
    if (thePrevPrev)
      thePrevPrev->next = e;
    else
      first = e;
    if (last == e) last = thePrev;
    thePrev->oh = e->oh;
    thePrev->cum_prod = e->cum_prod;
    e->oh -= thePrev->getQuantity();
    if (thePrev->getQuantity() > 0) e->cum_prod -= thePrev->getQuantity();
  }

  // Update the onhand for all later events
  if (fabs(delta) > ROUNDING_ERROR)
  {
    double cumdelta = (oldqty>0? oldqty : 0) - (newqty>0 ? newqty : 0);
    if (fabs(cumdelta) > 0)
      for (iterator i=begin(e); i!=end(); ++i)
      {
        i->oh -= delta;
        i->cum_prod -= cumdelta;
      }
    else
      for (iterator i=begin(e); i!=end(); ++i)
        i->oh -= delta;
  }

  // Final debugging check commented out, since loadplans change in pairs.
  // After changing the first one the second is affected too but not
  // repositioned yet...
  //assert(check());
}


template <class type> bool TimeLine<type>::check() const
{
  double expectedOH = 0.0;
  double expectedCumProd = 0.0;
  const Event *prev = NULL;
  for (const_iterator i = begin(); i!=end(); ++i)
  {
    // Problem 1: The onhands don't add up properly
    expectedOH += i->getQuantity();
    if (i->getQuantity() > 0) expectedCumProd += i->getQuantity();
    if (fabs(expectedOH - i->oh) > ROUNDING_ERROR)
    {
      inspect("Error: timeline onhand value corrupted on "
          + string(i->getDate()));
      return false;
    }
    // Problem 2: The cumulative produced quantity isn't correct
    if (fabs(expectedCumProd - i->cum_prod) > ROUNDING_ERROR)
    {
      inspect("Error: timeline cumulative produced value corrupted on "
          + string(i->getDate()));
      return false;
    }
    // Problem 3: Timeline is not sorted correctly
    if (prev && !(*prev<*i)
        && fabs(prev->getQuantity() - i->getQuantity())>ROUNDING_ERROR)
    {
      inspect("Error: timeline sort corrupted on " + string(i->getDate()));
      return false;
    }
    prev = &*i;
  }
  return true;
}

} // end namespace
} // end namespace
#endif


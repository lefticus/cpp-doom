//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//  MapObj data. Map Objects or mobjs are actors, entities,
//  thinker, take-your-pick... anything that moves, acts, or
//  suffers state changes of more or less violent nature.
//


#ifndef __D_THINK__
#define __D_THINK__

#include <tuple>



//
// Experimental stuff.
// To compile this as "ANSI C with classes"
//  we will need to handle the various
//  action functions cleanly.
//

struct mobj_t;
struct player_t;
struct pspdef_t;
struct thinker_t;

typedef void (*actionf_t1)(thinker_t *mo);
typedef void (*actionf_m1)(mobj_t *mo);
typedef void (*actionf_p3)(mobj_t *mo, player_t *player, pspdef_t *psp);

struct actionf_t {
  constexpr actionf_t() = default;

  explicit constexpr actionf_t(actionf_m1 p)
  {
    std::get<actionf_m1>(data) = p;
  }

  explicit constexpr actionf_t(actionf_t1 p)
  {
    std::get<actionf_t1>(data) = p;
  }

  explicit constexpr actionf_t(actionf_p3 p)
  {
    std::get<actionf_p3>(data) = p;
  }

  constexpr actionf_t(int        p)
  {
    std::get<int>(data) = p;
  }

  constexpr actionf_t &operator=(actionf_m1 p) {
    data = actionf_t{p}.data;
    return *this;
  }

  constexpr actionf_t &operator=(actionf_t1 p) {
    data = actionf_t{p}.data;
    return *this;
  }

  constexpr actionf_t &operator=(actionf_p3 p) {
    data = actionf_t{p}.data;
    return *this;
  }

  constexpr actionf_t &operator=(const void *p) {
    data = actionf_t{}.data;
    std::get<const void *>(data) = p;
    return *this;
  }

  [[nodiscard]] constexpr explicit operator const void *() {
    return std::get<const void *>(data);
  }

  template <typename... Param>
  [[nodiscard]] constexpr bool operator==(void (*p)(Param...)) const {
    return std::get<decltype(p)>(data) == p;
  }

   bool call_if( thinker_t *thinker ){
      return call_iff( thinker ) ||
         call_iff( reinterpret_cast<mobj_t*>(thinker) ) ||
         call_iff( reinterpret_cast<mobj_t*>(thinker),
                   (player_t*)nullptr,
                   (pspdef_t*)nullptr );
   }

   bool call_if( mobj_t *thinker ){
      return call_iff( thinker ) ||
         call_iff( reinterpret_cast<mobj_t*>(thinker) ) ||
         call_iff( reinterpret_cast<mobj_t*>(thinker),
                   (player_t*)nullptr,
                   (pspdef_t*)nullptr );
   }

   bool call_if( mobj_t *mo, player_t *player, pspdef_t *psp ){
      return call_iff( mo ) ||
         call_iff( mo, player, psp );
   }

   constexpr explicit operator bool() const {
    return *this != actionf_t{};
  }

  constexpr bool operator==(const actionf_t &) const = default;

private:
   std::tuple<int, const void *,
              actionf_t1,
              actionf_m1,
              actionf_p3>
      data{};
};




// Historically, "think_t" is yet another
//  function pointer to a routine to handle
//  an actor.
typedef actionf_t  think_t;


// Doubly linked list of actors.
struct thinker_t
{
    struct thinker_t*	prev;
    struct thinker_t*	next;
    think_t		function;
    
};



#endif

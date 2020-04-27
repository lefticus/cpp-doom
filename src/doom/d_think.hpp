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
struct ceiling_t;
struct fire_t;
struct vldoor_t;
struct fireflicker_t;
struct plat_t;
struct lightflash_t;
struct floormove_t;
struct strobe_t;
struct glow_t;
struct thinker_t;

typedef void (*actionf_v)();
typedef void (*actionf_f1)(fire_t *mo);
typedef void (*actionf_fm1)(floormove_t *mo);
typedef void (*actionf_c1)(ceiling_t *mo);
typedef void (*actionf_g1)(glow_t *mo);
typedef void (*actionf_s1)(strobe_t *mo);
typedef void (*actionf_v1)(vldoor_t *mo);
typedef void (*actionf_pp1)(plat_t *mo);
typedef void (*actionf_lf1)(lightflash_t *mo);
typedef void (*actionf_ff1)(fireflicker_t *mo);
typedef void (*actionf_t1)(thinker_t *mo);
typedef void (*actionf_p1)(mobj_t *mo);
typedef void (*actionf_p2)(player_t *player, pspdef_t *psp );
typedef void (*actionf_p3)(mobj_t *mo, player_t *player, pspdef_t *psp); // [crispy] let pspr action pointers get called from mobj states

struct actionf_t {
  constexpr actionf_t() = default;

  template<typename Ret, typename ... Param>
  explicit constexpr actionf_t(Ret (*p)(Param...))
  {
    std::get<decltype(p)>(data) = p;
  }

  template <typename Ret, typename... Param>
  constexpr actionf_t &operator=(Ret (*p)(Param...)) {
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

  template <typename Ret, typename... Param>
  [[nodiscard]] constexpr bool operator==(Ret (*p)(Param...)) const {
    return std::get<decltype(p)>(data) == p;
  }

  template <typename... Param> constexpr bool call_iff(Param... param) {
    const auto func = std::get<void (*)(Param...)>(data);
    if (func) {
      func(param...);
      return true;
    } else {
      return false;
    }
  }

   template<typename T>
   bool call_if( T *thinker ){
      return call_iff<>() ||
         call_iff<fire_t *>( (fire_t *)thinker ) ||
         call_iff<floormove_t *>( (floormove_t *)thinker ) ||
         call_iff<ceiling_t *>( (ceiling_t *)thinker ) ||
         call_iff<glow_t *>( (glow_t *)thinker ) ||
         call_iff<strobe_t *>( (strobe_t *)thinker ) ||
         call_iff<vldoor_t *>( (vldoor_t *)thinker ) ||
         call_iff<plat_t *>( (plat_t *)thinker ) ||
         call_iff<lightflash_t *>( (lightflash_t *)thinker ) ||
         call_iff<fireflicker_t *>( (fireflicker_t *)thinker ) ||
         call_iff<thinker_t *>( (thinker_t*)thinker ) ||
         call_iff<mobj_t *>( (mobj_t *)thinker ) ||
         call_iff<player_t *, pspdef_t *>( (player_t *)thinker, (pspdef_t*)nullptr ) ||
         call_iff<mobj_t *, player_t *, pspdef_t *>( (mobj_t *)thinker, (player_t*)nullptr, (pspdef_t*)nullptr );
   }

   bool call_if( mobj_t *thinker, player_t *player, pspdef_t *psp ){
       return call_iff<>() ||
          call_iff<fire_t *>( (fire_t *)thinker ) ||
          call_iff<floormove_t *>( (floormove_t *)thinker ) ||
          call_iff<ceiling_t *>( (ceiling_t *)thinker ) ||
          call_iff<glow_t *>( (glow_t *)thinker ) ||
          call_iff<strobe_t *>( (strobe_t *)thinker ) ||
          call_iff<vldoor_t *>( (vldoor_t *)thinker ) ||
          call_iff<plat_t *>( (plat_t *)thinker ) ||
          call_iff<lightflash_t *>( (lightflash_t *)thinker ) ||
          call_iff<fireflicker_t *>( (fireflicker_t *)thinker ) ||
          call_iff<thinker_t *>( (thinker_t*)thinker ) ||
          call_iff<mobj_t *>( thinker ) ||
          call_iff<player_t *, pspdef_t *>( (player_t *)thinker, (pspdef_t*)psp ) ||
          call_iff<mobj_t *, player_t *, pspdef_t *>( thinker, (player_t*)player, (pspdef_t*)psp );
   }

  constexpr explicit operator bool() const {
    return *this != actionf_t{};
  }

  constexpr bool operator==(const actionf_t &) const = default;

private:
  std::tuple<const void *, actionf_t1, actionf_g1, actionf_s1, actionf_fm1, actionf_v, actionf_c1, actionf_f1,
             actionf_lf1, actionf_ff1, actionf_v1, actionf_p1, actionf_pp1,
             actionf_p2, actionf_p3>
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

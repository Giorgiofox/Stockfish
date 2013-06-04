/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2013 Marco Costalba, Joona Kiiski, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <cassert>

#include "bitboard.h"
#include "bitcount.h"
#include "knowledgebase.h"
#include "movegen.h"

using std::string;

namespace {

  const int corner_dist[64]={
      0, 1, 2, 3, 3, 2, 1, 0,
      1, 1, 2, 3, 3, 2, 1, 1,
      2, 2, 2, 3, 3, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3, 3, 3, 3,
      2, 2, 2, 3, 3, 2, 2, 2,
      1, 1, 2, 3, 3, 2, 1, 1,
      0, 1, 2, 3, 3, 2, 1, 0
    };

  // Get the material key of a Position out of the given endgame key code
  // like "KBPKN". The trick here is to first forge an ad-hoc fen string
  // and then let a Position object to do the work for us. Note that the
  // fen string could correspond to an illegal position.
  Key key(const string& code, Color c) {

    assert(code.length() > 0 && code.length() < 8);
    assert(code[0] == 'K');

    string sides[] = { code.substr(code.find('K', 1)),      // Weaker
                       code.substr(0, code.find('K', 1)) }; // Stronger

    std::transform(sides[c].begin(), sides[c].end(), sides[c].begin(), tolower);

    string fen =  sides[0] + char('0' + int(8 - code.length()))
                + sides[1] + "/8/8/8/8/8/8/8 w - - 0 10";

    return Position(fen, false, NULL).material_key();
  }

  bool genericDraw(const Position& pos, Value &v)
  {
    ((void)(pos)); // Disable unused variable waring
    v=VALUE_DRAW;
    return true;
  }

  bool drawIfKingNotInCorner(const Position& pos, Value &v)
  {
    const Bitboard corners=SquareBB[SQ_A1] | SquareBB[SQ_A8] |  SquareBB[SQ_H1] |  SquareBB[SQ_H8];
    return (corners & pos.pieces(KING)) ? false : genericDraw(pos,v);
  }

  template<Color strongerSide>
  bool KBBK(const Position& pos, Value &v)
  {
    Color weakerSide=~strongerSide;
    Value result;
    Bitboard bishops=pos.pieces(strongerSide, BISHOP);
    Square loserKSq = pos.king_square(weakerSide);
    
    // Don't use this function if weaker side king can capture a bishop
    if(bishops & pos.attacks_from<KING>(loserKSq))
      return false;

    result = ( popcount<Max15>(bishops & BlackSquares)!=1 ) ? VALUE_DRAW // The endgame KBBK is drawn if the bishops cover squares of a single color only
            : 2*BishopValueMg + 250 - 25*corner_dist[loserKSq] - 12*square_distance(loserKSq,pos.king_square(strongerSide));

    v=(strongerSide == pos.side_to_move()) ? result : -result;
    return true;
  }
  
  /// KP vs K. This endgame is evaluated with the help of a bitbase.
  template<Color strongerSide>
  bool KPK(const Position& pos, Value &v) {
    Color weakerSide=~strongerSide;
    ((void)(weakerSide)); // Disable unused variable waring
  
    assert(pos.non_pawn_material(strongerSide) == VALUE_ZERO);
    assert(pos.non_pawn_material(weakerSide) == VALUE_ZERO);
    assert(pos.piece_count(strongerSide, PAWN) == 1);
    assert(pos.piece_count(weakerSide, PAWN) == 0);
  
    Square wksq, bksq, wpsq;
    Color us;
  
    if (strongerSide == WHITE)
    {
        wksq = pos.king_square(WHITE);
        bksq = pos.king_square(BLACK);
        wpsq = pos.piece_list(WHITE, PAWN)[0];
        us   = pos.side_to_move();
    }
    else
    {
        wksq = ~pos.king_square(BLACK);
        bksq = ~pos.king_square(WHITE);
        wpsq = ~pos.piece_list(BLACK, PAWN)[0];
        us   = ~pos.side_to_move();
    }
  
    if (file_of(wpsq) >= FILE_E)
    {
        wksq = mirror(wksq);
        bksq = mirror(bksq);
        wpsq = mirror(wpsq);
    }
  
    if (!Bitbases::probe_kpk(wksq, wpsq, bksq, us))
        return VALUE_DRAW;
  
    Value result = VALUE_KNOWN_WIN + PawnValueEg + Value(rank_of(wpsq));
  
    v=(strongerSide == pos.side_to_move()) ? result : -result;
    return true;
  }

} // namespace

void KnowledgeBases::add(const string& code,KnowledgeProbeFunction func) {
  m[key(code, WHITE)]=func;
  m[key(code, BLACK)]=func;
}

KnowledgeBases::KnowledgeBases()
{
  m[key("KK", WHITE)]=genericDraw;
  m[key("KK", BLACK)]=genericDraw;
  m[key("KBK", WHITE)]=genericDraw;
  m[key("KBK", BLACK)]=genericDraw;
  m[key("KNK", WHITE)]=genericDraw;
  m[key("KNK", BLACK)]=genericDraw;
  m[key("KPK", WHITE)]=KPK<WHITE>;
  m[key("KPK", BLACK)]=KPK<BLACK>;
  
  m[key("KBKB", WHITE)]=drawIfKingNotInCorner;
  m[key("KBKB", BLACK)]=drawIfKingNotInCorner;
  m[key("KBKN", WHITE)]=drawIfKingNotInCorner;
  m[key("KBKN", BLACK)]=drawIfKingNotInCorner;
  m[key("KNKN", WHITE)]=drawIfKingNotInCorner;
  m[key("KNKN", BLACK)]=drawIfKingNotInCorner;
  m[key("KNNK", WHITE)]=drawIfKingNotInCorner;
  m[key("KNNK", BLACK)]=drawIfKingNotInCorner;

  m[key("KBBK", WHITE)]=KBBK<WHITE>;
  m[key("KBBK", BLACK)]=KBBK<BLACK>;

}





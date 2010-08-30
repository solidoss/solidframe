/* Implementation file utility.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "utility/workpool.hpp"
#include "utility/string.hpp"
#include "utility/polycontainer.hpp"
#include <cstring>
#ifndef ON_WINDOWS
#include <strings.h>
#endif


//------	WorkPoolPlugin --------------------------------------------

WorkPoolPlugin::~WorkPoolPlugin(){
}
void WorkPoolPlugin::prepare(){
}
void WorkPoolPlugin::unprepare(){
}
bool WorkPoolPlugin::release()const{
	return false;//do not delete
}

static WorkPoolPlugin pwp;

WorkPoolPlugin* basicWorkPoolPlugin(){
	return &pwp;
}

//------	PolyContainter --------------------------------------------

static const char* strs[] = {
 "Ox00", "Ox01", "Ox02", "Ox03", "Ox04", "Ox05", "Ox06", "Ox07", "Ox08",  "TAB",
   "LF", "Ox0B", "Ox0C",   "CR", "Ox0E", "Ox0F", "Ox10", "Ox11", "Ox12", "Ox13",
 "Ox14", "Ox15", "Ox16", "Ox17", "Ox18", "Ox19", "Ox1A", "Ox1B", "Ox1C", "Ox1D",
 "Ox1E", "Ox1F","SPACE",    "!",    "\"",    "#",    "$",    "%",    "&",    "'",
    "(",    ")",    "*",    "+",    ",",    "-",    ".",    "/",    "0",    "1",
    "2",    "3",    "4",    "5",    "6",    "7",    "8",    "9",    ":",    ";",
    "<",    "=",    ">",    "?",    "@",    "A",    "B",    "C",    "D",    "E",
    "F",    "G",    "H",    "I",    "J",    "K",    "L",    "M",    "N",    "O",
    "P",    "Q",    "R",    "S",    "T",    "U",    "V",    "W",    "X",    "Y",
    "Z",    "[",    "\\",    "]",    "^",    "_",    "`",    "a",    "b",    "c",
    "d",    "e",    "f",    "g",    "h",    "i",    "j",    "k",    "l",    "m",
    "n",    "o",    "p",    "q",    "r",    "s",    "t",    "u",    "v",    "w",
    "x",    "y",    "z",    "{",    "|",    "}",    "~", "Ox7F", "Ox80", "Ox81",
 "Ox82", "Ox83", "Ox84", "Ox85", "Ox86", "Ox87", "Ox88", "Ox89", "Ox8A", "Ox8B",
 "Ox8C", "Ox8D", "Ox8E", "Ox8F", "Ox90", "Ox91", "Ox92", "Ox93", "Ox94", "Ox95",
 "Ox96", "Ox97", "Ox98", "Ox99", "Ox9A", "Ox9B", "Ox9C", "Ox9D", "Ox9E", "Ox9F",
 "OxA0", "OxA1", "OxA2", "OxA3", "OxA4", "OxA5", "OxA6", "OxA7", "OxA8", "OxA9",
 "OxAA", "OxAB", "OxAC", "OxAD", "OxAE", "OxAF", "OxB0", "OxB1", "OxB2", "OxB3",
 "OxB4", "OxB5", "OxB6", "OxB7", "OxB8", "OxB9", "OxBA", "OxBB", "OxBC", "OxBD",
 "OxBE", "OxBF", "OxC0", "OxC1", "OxC2", "OxC3", "OxC4", "OxC5", "OxC6", "OxC7",
 "OxC8", "OxC9", "OxCA", "OxCB", "OxCC", "OxCD", "OxCE", "OxCF", "OxD0", "OxD1",
 "OxD2", "OxD3", "OxD4", "OxD5", "OxD6", "OxD7", "OxD8", "OxD9", "OxDA", "OxDB",
 "OxDC", "OxDD", "OxDE", "OxDF", "OxE0", "OxE1", "OxE2", "OxE3", "OxE4", "OxE5",
 "OxE6", "OxE7", "OxE8", "OxE9", "OxEA", "OxEB", "OxEC", "OxED", "OxEE", "OxEF",
 "OxF0", "OxF1", "OxF2", "OxF3", "OxF4", "OxF5", "OxF6", "OxF7", "OxF8", "OxF9",
 "OxFA", "OxFB", "OxFC", "OxFD", "OxFE", "OxFF"
};


const char * charToString(unsigned _c){
	return strs[_c & 255];
}


/*static*/ int cstring::cmp(const char* _s1, const char *_s2){
    return ::strcmp(_s1, _s2);
}

/*static*/ int cstring::ncmp(const char* _s1, const char *_s2, uint _len){
    return ::strncmp(_s1, _s2, _len);
}

/*static*/ int cstring::casecmp(const char* _s1, const char *_s2){
#ifdef ON_WINDOWS
        return _stricmp(_s1,_s2);
#else
        return ::strcasecmp(_s1,_s2);
#endif
}

/*static*/ int cstring::ncasecmp(const char* _s1, const char *_s2, uint _len){
#ifdef ON_WINDOWS
        return _strnicmp(_s1,_s2, _len);
#else
        return strncasecmp(_s1,_s2, _len);
#endif
}



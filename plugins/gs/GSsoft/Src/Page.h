/*  GSsoft
 *  Copyright (C) 2002-2005  GSsoft Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __PAGE_H__
#define __PAGE_H__

u32 getPixelAddress32(int x, int y, u32 bp, u32 bw);
u32 getPixelAddress16(int x, int y, u32 bp, u32 bw);
u32 getPixelAddress16S(int x, int y, u32 bp, u32 bw);
u32 getPixelAddress8(int x, int y, u32 bp, u32 bw);
u32 getPixelAddress4(int x, int y, u32 bp, u32 bw);

u32 getPixelAddress32Z(int x, int y, u32 bp, u32 bw);
u32 getPixelAddress16Z(int x, int y, u32 bp, u32 bw);
u32 getPixelAddress16SZ(int x, int y, u32 bp, u32 bw);


#endif /* __PAGE_H__ */

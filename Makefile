################################################################################
# 
# Copyright(c) Realtek Semiconductor Corp. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation; either version 2 of the License, or (at your option) 
# any later version.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT 
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
# more details.
# 
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 
# Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# 
# The full GNU General Public License is included in this distribution in the
# file called LICENSE.
# 
################################################################################
ARCH=arm

all:  clean modules cp

modules:
	$(MAKE) -C src/ modules

clean:
	$(MAKE) -C src/ clean
                                                                                                                             
install:
	$(MAKE) -C src/ install

cp:
	#rm /WorkSpace/ARM/filesystem/s3c2440_recover/Dri_init.ko
	#cp src/Dri_init.ko /WorkSpace/ARM/filesystem/s3c2440_recover

#################################################################################
## make ARCH=arm CC=arm-none-linux-gnueabi-gcc LD=arm-none-linux-gnueabi-ld KLIB=/home/ema/source/linux-03.00.01.06 KLIB_BUILD=/home/ema/source/linux-03.00.01.06




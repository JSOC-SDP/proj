      subroutine mflux_ll(m,n,a,b,c,d,rsun,brll,mflux)
c+
c - -  Purpose:  Compute net magnetic flux given the array brce of photospheric
c                radial magnetic field values, given in lon,lat index order.
c
c - -  Usage:    call mflux_ll(m,n,a,b,c,d,rsun,brll,mflux)
c
c - -  Input:    m,n - integers denoting the number of cell-centers in the
c                colatitude and longitude directions, respectively
c
c - -  Input:    a,b - the real*8 values of colatitude (theta) 
c                at the northern and southern edges of the problem boundary
c                [radians]
c
c - -  Input:    c,d - the real*8 values of longitude edges [radians]
c
c - -  Input:    rsun - real*8 value of radius of Sun [km]. Normally 6.96d5.
c
c - -  Input:    brll(n,m) - real*8 array of cell-center (CE grid) 
c                radial magnetic field values [G] in lon,lat index order
c
c - -  Output:   mflux - real*8 value of the net magnetic flux [G km^2]
c
c - -  Note:     To convert mflux to Maxwells, multiply mflux by 1d10. 
c
c-
c   PDFI_SS Electric Field Inversion Software
c   http://cgem.ssl.berkeley.edu/cgi-bin/cgem/PDFI_SS/index
c   Copyright (C) 2015-2018 University of California
c  
c   This software is based on the concepts described in Kazachenko et al. 
c   (2014, ApJ 795, 17).  It also extends those techniques to 
c   spherical coordinates, and uses a staggered, rather than a centered grid.
c   If you use the software in a scientific publication, 
c   the authors would appreciate a citation to this paper and any future papers 
c   describing updates to the methods.
c  
c   This is free software; you can redistribute it and/or
c   modify it under the terms of the GNU Lesser General Public
c   License as published by the Free Software Foundation;
c   either version 2.1 of the License, or (at your option) any later version.
c  
c   This software is distributed in the hope that it will be useful,
c   but WITHOUT ANY WARRANTY; without even the implied warranty of
c   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
c   See the GNU Lesser General Public License for more details.
c  
c   To view the GNU Lesser General Public License visit
c   http://www.gnu.org/copyleft/lesser.html
c   or write to the Free Software Foundation, Inc.,
c   59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
c
      implicit none
c
c - - input calling arguments:
c
      integer :: m,n
      real*8 :: a,b,c,d,rsun
      real*8 :: brll(n,m)
c
c - - output arguments:
c
      real*8 :: mflux
c
c - - local variables:
c
      real*8 :: brce(m,n)
      real*8 :: flux
c
      call bryeell2tp_ss(m,n,brll,brce)
      call mflux_ss(m,n,a,b,c,d,rsun,brce,flux)
c
      mflux=flux
c
c - - we're done
c
      return
      end

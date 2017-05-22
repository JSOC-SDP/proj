      subroutine brll2tp_ss(m,n,brllcoe,brtpcoe)
c
c+
c   Purpose:  To transpose B_r data array from lon,lat to theta,phi order.
c             Can also be used to transpose other scalar COE arrays.
c     Usage:  call brll2tp_ss(m,n,brllcoe,brtpcoe)
c     Input:  m,n - number of cell centers in the theta (lat), and phi (lon)
c             directions, respectively.
c     Input:  brllcoe(n+1,m+1) - array of the radial component of the
c             magnetic field evaluated at COE locations
c             (corners plus exterior corners on boundary).
c    Output:  brtpcoe(m+1,n+1) - array of radial magnetic field component
c             stored in theta,phi index order.
c-
c   PDFI_SS Electric Field Inversion Software
c   http://cgem.ssl.berkeley.edu/cgi-bin/cgem/PDFI_SS/index
c   Copyright (C) 2015,2016 University of California
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
      integer :: m,n
      real*8 :: brllcoe(n+1,m+1)
c
      real*8 :: brtpcoe(m+1,n+1)
c
      integer :: i,j
      do i=1,m+1
         do j=1,n+1
            brtpcoe(i,j)=brllcoe(j,m+2-i)
         enddo
      enddo
c
      return
      end
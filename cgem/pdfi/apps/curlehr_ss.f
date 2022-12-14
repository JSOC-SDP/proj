      subroutine curlehr_ss(m,n,rsun,a,b,c,d,et,ep,brt)
c
c+
c - - Purpose: Compute radial component of the curl of c*E_h.
c              This can be used to check inductivity of solutions for c*E_h.
c
c - - Usage:   call curlehr_ss(m,n,rsun,a,b,c,d,et,ep,brt)
c
c - - Input:   m,n - number of cell centers in theta, phi directions, resp.
c
c - - Input:   rsun - real*8 value of assumed radius of Sun [km].
c              Normally 6.96d5.
c
c - - Input:   a,b - real*8 values of colatitude limits of wedge domain
c              (a < b) [radians]
c
c - - Input:   c,d - real*8 values of longitude limits of wedge domain 
c              (c < d) [radians]
c
c - - Input:   et(m,n+1) - real*8 array of theta component of ptd electric 
c              field, multiplied by the speed of light (cE) [G km/s]
c
c - - Input:   ep(m+1,n) - real*8 array of phi component of ptd electric field, 
c              multiplied by the speed of light (cE) [G km/s]
c
c - - Output:  brt(m,n) - real*8 array of the radial component of curl c*E_h
c              [G /sec]
c-
c - -  PDFI_SS electric field inversion software
c - -  http://cgem.ssl.berkeley.edu/~fisher/public/software/PDFI_SS
c - -  Copyright (C) 2015-2019 Regents of the University of California
c 
c - -  This software is based on the concepts described in Kazachenko et al.
c - -  (2014, ApJ 795, 17).  A detailed description of the software is in
c - -  Fisher et al. (2019, arXiv:1912.08301 ).
c - -  If you use the software in a scientific 
c - -  publication, the authors would appreciate a citation to these papers 
c - -  and any future papers describing updates to the methods.
c
c - -  This is free software; you can redistribute it and/or
c - -  modify it under the terms of the GNU Lesser General Public
c - -  License as published by the Free Software Foundation,
c - -  version 2.1 of the License.
c
c - -  This software is distributed in the hope that it will be useful,
c - -  but WITHOUT ANY WARRANTY; without even the implied warranty of
c - -  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
c - -  See the GNU Lesser General Public License for more details.
c
c - -  To view the GNU Lesser General Public License visit
c - -  http://www.gnu.org/copyleft/lesser.html
c - -  or write to the Free Software Foundation, Inc.,
c - -  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
c
      implicit none
c
c - - Input variables:
c
      integer :: m,n
      real*8 :: rsun,a,b,c,d
      real*8 :: et(m,n+1),ep(m+1,n)
c
c - - Output variables:
c
      real*8 :: brt(m,n)
c
c - - Local variables:
c
      real*8 :: curlr(m,n)
      real*8 :: sinth(m+1),sinth_hlf(m),dtheta,dphi
c
      dtheta=(b-a)/m
      dphi=(d-c)/n
c
      call sinthta_ss(a,b,m,sinth,sinth_hlf)
c
      call curlh_ce_ss(m,n,et,ep,rsun,sinth,sinth_hlf,dtheta,
     1 dphi, curlr)
c
      brt(1:m,1:n) = curlr(1:m,1:n)
c
      return
      end

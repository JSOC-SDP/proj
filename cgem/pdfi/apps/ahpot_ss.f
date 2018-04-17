       subroutine ahpot_ss(m,n,p,a,b,c,d,rsun,rssmrs,scrb3d,atpot,appot)
c
c+ - - Purpose: compute potential field value of at,ap (vector potential
c               components) given the 3-d array scrb3d by taking curl of 
c               scriptB rhat. scrb3d is computed by subroutine scrbpot_ss.
c
c - -  Usage:   call ahpot_ss(m,n,p,a,b,c,d,rsun,rssmrs,scrb3d,atpot,appot)
c
c - -  Input:   m,n,p: integer values of numbers of cell centers in theta,
c               phi, and r directions
c - -  Input:   a,b,c,d:  real*8 values of min, max colatitude, min, max
c               values of longitude. [radians]
c - -  Input:   rsun,rssmrs: real*8 values of the radius of sun, and 
c               distance from phot to source surface. [km] Normally rsun=6.96d5.
c - -  Input:   scrb3d(m,n,p+1): real*8 array of poloidal potential scribtb
c               [G km^2]
c - -  Output:  atpot(m,n+1,p+1): real*8 array of theta-comp of vector potential
c               [G km]
c - -  Output:  appot(m+1,n,p+1): real*8 array of phi-comp of vector potential
c               [G km]
c - -  Note:    atpot,appot are computed on phi and theta edges, and on
c -             radial shells.
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
c - - input variables:
c
      integer :: m,n,p
      real*8 :: a,b,c,d,rsun,rssmrs
      real*8 :: scrb3d(m,n,p+1)
c 
c - - output variables:
c
      real*8 :: atpot(m,n+1,p+1)
      real*8 :: appot(m+1,n,p+1)
c
c - - local variables:
c
      integer :: q
      real*8 :: scrb(m+2,n+2),bdas(n),bdbs(n),bdcs(m),bdds(m)
      real*8 :: sinth(m+1),sinth_hlf(m),r(p+1)
      real*8 :: curlt(m,n+1),curlp(m+1,n)
      real*8 :: dphi,dtheta,delr
c
      bdas(1:n)=0.d0
      bdbs(1:n)=0.d0
      bdcs(1:m)=0.d0
      bdds(1:m)=0.d0
c
      dtheta=(b-a)/m
      dphi=(d-c)/n
      call sinthta_ss(a,b,m,sinth,sinth_hlf)
      delr=rssmrs/p
c
c - - define radial shell arrays:
c
      r(1)=rsun
      do q=2,p+1
         r(q)=r(1)+(q-1)*delr
      end do
c
      do q=1,p+1
c - - get script B on radial shells, set equal to scrb array:
         scrb(2:m+1,2:n+1)=scrb3d(1:m,1:n,q)
c - - fill in ghost zones in the shell surface:
         scrb(1,2:n+1)=scrb(2,2:n+1)-1.d0*dtheta*bdas(1:n)
         scrb(m+2,2:n+1)=scrb(m+1,2:n+1)+1.d0*dtheta*bdbs(1:n)
c        scrb(2:m+1,1)=scrb(2:m+1,2)-1.d0*dphi*bdcs(1:m)
c        scrb(2:m+1,n+2)=scrb(2:m+1,n+1)+1.d0*dphi*bdds(1:m)
c - -    Periodic BC in phi:
         scrb(2:m+1,1)=scrb(2:m+1,n+1)
         scrb(2:m+1,n+2)=scrb(2:m+1,2)
c
         call curl_psi_rhat_ce_ss(m,n,scrb,r(q),sinth_hlf,dtheta,
     1        dphi,curlt,curlp)
         atpot(1:m,1:n+1,q)=curlt(1:m,1:n+1)
         appot(1:m+1,1:n,q)=curlp(1:m+1,1:n)
      end do
c
      return
      end

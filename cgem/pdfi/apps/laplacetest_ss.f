      subroutine laplacetest_ss(m,n,p,bcn,a,b,c,d,rsun,rssmrs,psi3d,
     1 brh,brr)
c
c+ - - Purpose: Compute potential field value of delh^2 psi above 
c               photosphere in two different ways given the 3-d array psi3d, 
c               (1) by taking horizontal Laplacian at each radius value, and
c               (2) by taking radial contribution to Laplacian.  Both results 
c               should be equal, since according to Laplace equation, del^2 psi
c               =0.
c
c - -  Usage:   call laplacetest_ss(m,n,p,bcn,a,b,c,d,rsun,rssmrs,psi3d,brh,brr)
c
c - -  Input:   m,n,p: integer values of numbers of cell centers in theta,
c               phi, and r directions
c
c - -  Input:   bcn: integer flag for boundary conditions in phi: bcn=0 =>
c               periodic boundary conditions, bcn=3 => Neumann BC.
c
c - -  Input:   a,b,c,d:  real*8 values of min, max colatitude, min, max
c               values of longitude. [radians]
c
c - -  Input:   rsun,rssmrs: real*8 value for radius of sun, and distance 
c               from photosphere to source surface. [km].  Normally rsun=6.96e5.
c
c - -  Input:   psi3d(m,n,p+1): real*8 array of scalar potential psi
c               [G km]
c
c - -  Output:  brh(m,n,p-1): real*8 array of -delh^2 psi values
c               above photosphere and below the source-surface. [G / km]  
c               Computed from horizontal Laplacian.
c
c - -  Output:  brr(m,n,p-1): real*8 array of 1/r^2 d/dr (r^2 d psi/dr)
c               values above
c               photosphere and below source-surface. [G / km]  
c               Computed from radial contribution to Laplacian.
c
c - -  Note:    If Laplace equation is obeyed, then brh should be equal to brr
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
c - - input variables:
c
      integer :: m,n,p,bcn
      real*8 :: a,b,c,d,rsun,rssmrs
      real*8 :: psi3d(m,n,p+1)
c 
c - - output variables:
c
      real*8 :: brh(m,n,p-1),brr(m,n,p-1)
c
c - - local variables:
c
      integer :: q
      real*8 :: scrb(m+2,n+2),bdas(n),bdbs(n),bdcs(m),bdds(m)
      real*8 :: sinth(m+1),sinth_hlf(m),r(p+1),brshell(m,n)
      real*8 :: curlt(m,n+1),curlp(m+1,n)
      real*8 :: dphi,dtheta,delr,rqmh2,rqph2,rq2
c
      if((bcn .ne. 0) .and. (bcn .ne. 3)) then
         write(6,*) 'laplacetest_ss: Illegal bcn = ',bcn,' exiting'
         stop
      endif
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
c - - define edge and cell-center radius arrays:
c
      r(1)=rsun
      do q=2,p+1
         r(q)=r(1)+(q-1)*delr
      end do
c
      do q=2,p
         scrb(2:m+1,2:n+1)=psi3d(1:m,1:n,q)
         scrb(1,2:n+1)=scrb(2,2:n+1)-1.d0*dtheta*bdas(1:n)
         scrb(m+2,2:n+1)=scrb(m+1,2:n+1)+1.d0*dtheta*bdbs(1:n)
c
         if(bcn .eq. 3) then
c - -       Neumann BC in phi:
            scrb(2:m+1,1)=scrb(2:m+1,3)-2.d0*dphi*bdcs(1:m)
            scrb(2:m+1,n+2)=scrb(2:m+1,n)+2.d0*dphi*bdds(1:m)
         else
c - -       Periodic BC in phi:
            scrb(2:m+1,1)=scrb(2:m+1,n+1)
            scrb(2:m+1,n+2)=scrb(2:m+1,2)
         endif
c
         call curl_psi_rhat_ce_ss(m,n,scrb,r(q),sinth_hlf,dtheta,
     1        dphi,curlt,curlp)
         call curlh_ce_ss(m,n,curlt,curlp,r(q),sinth,sinth_hlf,dtheta,
     1        dphi,brshell)
         brh(1:m,1:n,q-1)=brshell(1:m,1:n)
         rqmh2=(r(q)-0.5d0*delr)**2
         rqph2=(r(q)+0.5d0*delr)**2
         rq2=r(q)**2
         brr(1:m,1:n,q-1)=(rqmh2*psi3d(1:m,1:n,q-1)+
     1                   rqph2*psi3d(1:m,1:n,q+1)
     1   -(rqmh2+rqph2)*psi3d(1:m,1:n,q))/(rq2*delr**2)
      end do
c
      return
      end

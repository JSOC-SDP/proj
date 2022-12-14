      subroutine enudge3d_gl_ll(m,n,rsun,dr,blont,blatt,brllt,elontop,
     1           elonbot,elattop,elatbot,erll)
c
c+
c - - Purpose: Given time derivatives of the 3 magnetic field components
c              (blont,blatt,brllt) through the corresponding voxel faces in 
c              a layer of voxels, computes the electric field on all edges 
c              of the spherical voxels.  Here, we assume global spherical 
c              geometry, rather than a spherical wedge domain.  In this 
c              subroutine the input arrays and output arrays are arranged into 
c              lon,lat order and orientation.
c
c - - Usage:   call enudge3d_gl_ll(m,n,rsun,dr,blont,blatt,brllt,
c              elontop,elonbot,elattop,elatbot,erll)
c
c - - Input:   m,n - integer values of number of cell centers in latitude,
c              and longitude, respectively.
c
c - - Input:   rsun - real*8 assumed value of the Sun's radius [km].
c
c - - Input:   dr - real*8 value of radial distance between top and bottom of
c              voxels [km].
c
c - - Input:   blont(n+1,m) - real*8 array of time derivatives of B_phi  - 
c              PE grid [G/sec]
c
c - - Input:   blatt(n,m+1) - real*8 array of time derivatives of B_theta- 
c              TE grid  [G/sec]
c
c - - Input:   brllt(n,m) - real*8 array of time derivatives of B_r - CE grid
c              [G/sec]
c
c - - Output:  elontop(n,m+1),elonbot(n,m+1) - real*8 arrays of c*E_lon - 
c              TE grid along the top and bottom rails of the voxels, 
c              respectively. [G km/sec]
c
c - - Output:  elattop(n+1,m),elatbot(n+1,m) - real*8 arrays of c*E_lat - 
c              PE grid along the top and bottom rails of the voxels, 
c              respectively. [G km/sec]
c
c - - Output:  erll(n+1,m+1) - real*8 array of c*E_r - COE grid - along 
c              the vertical rails of the voxels. [G km/sec]
c
c - - Note1:   The photospheric layer is assumed to lie halfway between the
c              top and bottom layer of voxels.  The top layer is at
c              R_S+0.5*dr, and the bottom layer is at R_s-0.5*dr.
c
c - - Note2:   FISHPACK's "global" boundary conditions assumed at the
c              N and S poles, and periodic boundary conditions assumed in 
c              longitude direction.
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
c
      implicit none
c
c - - variable declarations:
c
c - - calling arguments (input):
c
      integer :: m,n
      real*8 :: rsun,dr
      real*8 :: blatt(n,m+1),blont(n+1,m),brllt(n,m)
c
c - - calling arguments (output):
c
      real*8 :: elattop(n+1,m),elatbot(n+1,m),elontop(n,m+1),
     1          elonbot(n,m+1)
      real*8 :: erll(n+1,m+1)
c
c - - local variables:
c
      real*8 :: btt(m+1,n),bpt(m,n+1),brt(m,n)
      real*8 :: ettop(m,n+1),etbot(m,n+1),eptop(m+1,n),epbot(m+1,n)
      real*8 :: er(m+1,n+1)
c
c - - sdf variables:
c
c     integer*8 :: dims(20)
c
c - - Rotate input arrays from lon,lat to theta,phi order:
c
      call bryeell2tp_ss(m,n,brllt,brt)
      call bhyeell2tp_ss(m,n,blont,blatt,btt,bpt)
c
c - - call enudge3d_gl_ss with inputs in theta,phi order, and with output
c - - electric field arrays (local variables) in theta, phi order
c
      call enudge3d_gl_ss(m,n,rsun,dr,btt,bpt,brt,ettop,etbot,
     1     eptop,epbot,er)
c
c - - rotate electric field output arrays into lon,lat order from theta,phi
c - - order:
c
      call ehyeetp2ll_ss(m,n,ettop,eptop,elontop,elattop)
      call ehyeetp2ll_ss(m,n,etbot,epbot,elonbot,elatbot)
      call eryeetp2ll_ss(m,n,er,erll)
c
c - - we're done
c
      return
      end

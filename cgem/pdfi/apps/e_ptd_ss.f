      subroutine e_ptd_ss(m,n,scrbt,scrjt,rsun,sinth_hlf,
     1 dtheta,dphi,et,ep,er)
c
c+
c - - Purpose: Compute the 3 electric field components that solve Faraday's
c - - law, using the PTD decomposition.  Uses all 3 components of the time
c - - derivative of B.
c
c - - Usage:  call e_ptd_ss(m,n,scrbt,scrjt,rsun,sinth_hlf,dtheta,dphi,et,ep,er)
c
c - - Input:  m,n - number of cell centers in theta, phi directions, resp.
c - - Input:  scrbt(m+2,n+2) - solution to Poisson equation for d script b / dt
c - -         derived from calling ptdsolve_ss using input time derivs of B.
c - - Input:  scrjt(m+1,n+1) - solution to Poisson equation for d script j/ dt
c - -         derived from calling ptdsolve_ss using input time derivs of B.
c - - Input:  sinth_hlf(m) - sin(theta) evaluated at cell centers
c - - Output: et(m,n+1) - theta component of ptd electric field
c - - Output: ep(m+1,n) - phi component of ptd electric field
c - - Output: er(m+1,n+1) - radial component of ptd electric field
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
      real*8 :: rsun,dtheta,dphi
      real*8 :: scrbt(m+2,n+2),scrjt(m+1,n+1),et(m,n+1),ep(m+1,n),
     1 er(m+1,n+1),sinth_hlf(m)
      real*8 :: curlt(m,n+1),curlp(m+1,n)
c
      call curl_psi_rhat_ce_ss(m,n,scrbt,rsun,sinth_hlf,dtheta,
     1 dphi, curlt, curlp)
c
      et(1:m,1:n+1) = -curlt(1:m,1:n+1)
      ep(1:m+1,1:n) = -curlp(1:m+1,1:n)
      er(1:m+1,1:n+1)= -scrjt(1:m+1,1:n+1)
c
      return
      end

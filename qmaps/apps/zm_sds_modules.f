c#######################################################################
      module rp1d_def
c
c-----------------------------------------------------------------------
c ****** Define a structure to hold a REAL 1D pointer.
c-----------------------------------------------------------------------
c
      use number_types
c
      implicit none
c
      type :: rp1d
        real(r_typ), dimension(:), pointer :: f
      end type
c
      end module
c#######################################################################
      module sds_def
c
c-----------------------------------------------------------------------
c ****** Definition of the SDS data structure.
c-----------------------------------------------------------------------
c
      use number_types
      use rp1d_def
c
      implicit none
c
      integer, parameter, private :: mxdim=3
c
      type :: sds
        integer :: ndim
        integer, dimension(mxdim) :: dims
        logical :: scale
        logical :: hdf32
        type(rp1d), dimension(mxdim) :: scales
        real(r_typ), dimension(:,:,:), pointer :: f
      end type
c
      end module
c#######################################################################
      module rdhdf_1d_interface
      interface
        subroutine rdhdf_1d (fname,scale,nx,f,x,ierr)
        use number_types
        implicit none
        character(*) :: fname
        logical :: scale
        integer :: nx
        real(r_typ), dimension(:), pointer :: f
        real(r_typ), dimension(:), pointer :: x
        integer :: ierr
        end
      end interface
      end module
c#######################################################################
      module rdhdf_2d_interface
      interface
        subroutine rdhdf_2d (fname,scale,nx,ny,f,x,y,ierr)
        use number_types
        implicit none
        character(*) :: fname
        logical :: scale
        integer :: nx,ny
        real(r_typ), dimension(:,:), pointer :: f
        real(r_typ), dimension(:), pointer :: x,y
        integer :: ierr
        end
      end interface
      end module
c#######################################################################
      module rdhdf_3d_interface
      interface
        subroutine rdhdf_3d (fname,scale,nx,ny,nz,f,x,y,z,ierr)
        use number_types
        implicit none
        character(*) :: fname
        logical :: scale
        integer :: nx,ny,nz
        real(r_typ), dimension(:,:,:), pointer :: f
        real(r_typ), dimension(:), pointer :: x,y,z
        integer :: ierr
        end
      end interface
      end module
c#######################################################################
      module rdtxt_1d_interface
      interface
        subroutine rdtxt_1d (fname,scale,nx,f,x,ierr)
        use number_types
        implicit none
        character(*) :: fname
        logical :: scale
        integer :: nx
        real(r_typ), dimension(:), pointer :: f
        real(r_typ), dimension(:), pointer :: x
        integer :: ierr
        end
      end interface
      end module
c#######################################################################
      module rdtxt_2d_interface
      interface
        subroutine rdtxt_2d (fname,scale,nx,ny,f,x,y,ierr)
        use number_types
        implicit none
        character(*) :: fname
        logical :: scale
        integer :: nx,ny
        real(r_typ), dimension(:,:), pointer :: f
        real(r_typ), dimension(:), pointer :: x,y
        integer :: ierr
        end
      end interface
      end module
c#######################################################################
      module rdtxt_3d_interface
      interface
        subroutine rdtxt_3d (fname,scale,nx,ny,nz,f,x,y,z,ierr)
        use number_types
        implicit none
        character(*) :: fname
        logical :: scale
        integer :: nx,ny,nz
        real(r_typ), dimension(:,:,:), pointer :: f
        real(r_typ), dimension(:), pointer :: x,y,z
        integer :: ierr
        end
      end interface
      end module

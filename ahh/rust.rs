#![stable(feature = "rust1", since = "1.0.0")]


use core::any::Any;
use core::cell::CloneFromCell;
#[cfg(not(no_global_oom_handling))]
use core::clone::TrivialClone;
use core::clone::{CloneToUninit, UseCloned};
use core::cmp::Ordering;
use core::hash::{Hash, Hasher};
use core::intrinsics::abort;
#[cfg(not(no_global_oom_handling))]
use core::iter;
use core::marker::{PhantomData, Unsize};
use core::mem::{self, Alignment, ManuallyDrop};
use core::num::NonZeroUsize;
use core::ops::{CoerceUnsized, Deref, DerefMut, DerefPure, DispatchFromDyn, LegacyReceiver};
#[cfg(not(no_global_oom_handling))]
use core::ops::{Residual, Try};
use core::panic::{RefUnwindSafe, UnwindSafe};
use core::pin::{Pin, PinCoerceUnsized};
use core::ptr::{self, NonNull};
#[cfg(not(no_global_oom_handling))]
use core::slice::from_raw_parts_mut;
use core::sync::atomic::Ordering::{Acquire, Relaxed, Release};
use core::sync::atomic::{self, Atomic};
use core::{borrow, fmt, hint};

#[cfg(not(no_global_oom_handling))]
use crate::alloc::handle_alloc_error;
use crate::alloc::{AllocError, Allocator, Global, Layout};
use crate::borrow::{Cow, ToOwned};
use crate::boxed::Box;
use crate::rc::is_dangling;
#[cfg(not(no_global_oom_handling))]
use crate::string::String;
#[cfg(not(no_global_oom_handling))]
use crate::vec::Vec;
const INTERNAL_OVERFLOW_ERROR: &str = "Arc counter overflow";

#[cfg(not(sanitize = "thread"))]
macro_rules! acquire {
    ($x:expr) => {
        atomic::fence(Acquire)
    };
}

#[cfg(sanitize = "thread")]
macro_rules! acquire {
    ($x:expr) => {
        $x.load(Acquire)
    };
}

#[doc(search_unbox)]
#[rustc_diagnostic_item = "Arc"]
#[stable(feature = "rust1", since = "1.0.0")]
#[rustc_insignificant_dtor]
#[diagnostic::on_move(
    message = "the type `{Self}` does not implement `Copy`",
    label = "this move could be avoided by cloning the original `{Self}`, which is inexpensive",
    note = "consider using `Arc::clone`"
)]
pub struct Arc<
    T: ?Sized,
    #[unstable(feature = "allocator_api", issue = "32838")] A: Allocator = Global,
> {
    ptr: NonNull<ArcInner<T>>,
    phantom: PhantomData<ArcInner<T>>,
    alloc: A,
}

#[stable(feature = "rust1", since = "1.0.0")]
unsafe impl<T: ?Sized + Sync + Send, A: Allocator + Send> Send for Arc<T, A> {}
#[stable(feature = "rust1", since = "1.0.0")]
unsafe impl<T: ?Sized + Sync + Send, A: Allocator + Sync> Sync for Arc<T, A> {}

#[stable(feature = "catch_unwind", since = "1.9.0")]
impl<T: RefUnwindSafe + ?Sized, A: Allocator + UnwindSafe> UnwindSafe for Arc<T, A> {}

#[unstable(feature = "coerce_unsized", issue = "18598")]
impl<T: ?Sized + Unsize<U>, U: ?Sized, A: Allocator> CoerceUnsized<Arc<U, A>> for Arc<T, A> {}

#[unstable(feature = "dispatch_from_dyn", issue = "none")]
impl<T: ?Sized + Unsize<U>, U: ?Sized> DispatchFromDyn<Arc<U>> for Arc<T> {}

#[unstable(feature = "cell_get_cloned", issue = "145329")]
unsafe impl<T: ?Sized> CloneFromCell for Arc<T> {}

impl<T: ?Sized> Arc<T> {
    unsafe fn from_inner(ptr: NonNull<ArcInner<T>>) -> Self {
        unsafe { Self::from_inner_in(ptr, Global) }
    }

    unsafe fn from_ptr(ptr: *mut ArcInner<T>) -> Self {
        unsafe { Self::from_ptr_in(ptr, Global) }
    }
}

impl<T: ?Sized, A: Allocator> Arc<T, A> {
    #[inline]
    fn into_inner_with_allocator(this: Self) -> (NonNull<ArcInner<T>>, A) {
        let this = mem::ManuallyDrop::new(this);
        (this.ptr, unsafe { ptr::read(&this.alloc) })
    }

    #[inline]
    unsafe fn from_inner_in(ptr: NonNull<ArcInner<T>>, alloc: A) -> Self {
        Self { ptr, phantom: PhantomData, alloc }
    }

    #[inline]
    unsafe fn from_ptr_in(ptr: *mut ArcInner<T>, alloc: A) -> Self {
        unsafe { Self::from_inner_in(NonNull::new_unchecked(ptr), alloc) }
    }
}


#[stable(feature = "arc_weak", since = "1.4.0")]
unsafe impl<T: ?Sized + Sync + Send, A: Allocator + Send> Send for Weak<T, A> {}
#[stable(feature = "arc_weak", since = "1.4.0")]
unsafe impl<T: ?Sized + Sync + Send, A: Allocator + Sync> Sync for Weak<T, A> {}

#[unstable(feature = "coerce_unsized", issue = "18598")]
impl<T: ?Sized + Unsize<U>, U: ?Sized, A: Allocator> CoerceUnsized<Weak<U, A>> for Weak<T, A> {}
#[unstable(feature = "dispatch_from_dyn", issue = "none")]
impl<T: ?Sized + Unsize<U>, U: ?Sized> DispatchFromDyn<Weak<U>> for Weak<T> {}

#[unstable(feature = "cell_get_cloned", issue = "145329")]
unsafe impl<T: ?Sized> CloneFromCell for Weak<T> {}

#[stable(feature = "arc_weak", since = "1.4.0")]
impl<T: ?Sized, A: Allocator> fmt::Debug for Weak<T, A> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "(Weak)")
    }
}

#[repr(C, align(2))]
struct ArcInner<T: ?Sized> {
    strong: Atomic<usize>,

                weak: Atomic<usize>,

    data: T,
}

fn arcinner_layout_for_value_layout(layout: Layout) -> Layout {
                    Layout::new::<ArcInner<()>>().extend(layout).unwrap().0.pad_to_align()
}

unsafe impl<T: ?Sized + Sync + Send> Send for ArcInner<T> {}
unsafe impl<T: ?Sized + Sync + Send> Sync for ArcInner<T> {}



impl<T> Arc<[T]> {
                                                                                #[cfg(not(no_global_oom_handling))]
    #[inline]
    #[stable(feature = "new_uninit", since = "1.82.0")]
    #[must_use]
    pub fn new_uninit_slice(len: usize) -> Arc<[mem::MaybeUninit<T>]> {
        unsafe { Arc::from_ptr(Arc::allocate_for_slice(len)) }
    }

                                                                            #[cfg(not(no_global_oom_handling))]
    #[inline]
    #[stable(feature = "new_zeroed_alloc", since = "1.92.0")]
    #[must_use]
    pub fn new_zeroed_slice(len: usize) -> Arc<[mem::MaybeUninit<T>]> {
        unsafe {
            Arc::from_ptr(Arc::allocate_for_layout(
                Layout::array::<T>(len).unwrap(),
                |layout| Global.allocate_zeroed(layout),
                |mem| {
                    ptr::slice_from_raw_parts_mut(mem as *mut T, len)
                        as *mut ArcInner<[mem::MaybeUninit<T>]>
                },
            ))
        }
    }
}

impl<T, A: Allocator> Arc<[T], A> {
                                                                                                        #[cfg(not(no_global_oom_handling))]
    #[unstable(feature = "allocator_api", issue = "32838")]
    #[inline]
    pub fn new_uninit_slice_in(len: usize, alloc: A) -> Arc<[mem::MaybeUninit<T>], A> {
        unsafe { Arc::from_ptr_in(Arc::allocate_for_slice_in(len, &alloc), alloc) }
    }

                                                                                        #[cfg(not(no_global_oom_handling))]
    #[unstable(feature = "allocator_api", issue = "32838")]
    #[inline]
    pub fn new_zeroed_slice_in(len: usize, alloc: A) -> Arc<[mem::MaybeUninit<T>], A> {
        unsafe {
            Arc::from_ptr_in(
                Arc::allocate_for_layout(
                    Layout::array::<T>(len).unwrap(),
                    |layout| alloc.allocate_zeroed(layout),
                    |mem| {
                        ptr::slice_from_raw_parts_mut(mem.cast::<T>(), len)
                            as *mut ArcInner<[mem::MaybeUninit<T>]>
                    },
                ),
                alloc,
            )
        }
    }

                                                                    #[unstable(feature = "alloc_slice_into_array", issue = "148082")]
    #[inline]
    #[must_use]
    pub fn into_array<const N: usize>(self) -> Option<Arc<[T; N], A>> {
        if self.len() == N {
            let (ptr, alloc) = Self::into_raw_with_allocator(self);
            let ptr = ptr as *const [T; N];

                        let me = unsafe { Arc::from_raw_in(ptr, alloc) };
            Some(me)
        } else {
            None
        }
    }
}

impl<T, A: Allocator> Arc<mem::MaybeUninit<T>, A> {
                                                                                                            #[stable(feature = "new_uninit", since = "1.82.0")]
    #[must_use = "`self` will be dropped if the result is not used"]
    #[inline]
    pub unsafe fn assume_init(self) -> Arc<T, A> {
        let (ptr, alloc) = Arc::into_inner_with_allocator(self);
        unsafe { Arc::from_inner_in(ptr.cast(), alloc) }
    }
}

impl<T: ?Sized + CloneToUninit> Arc<T> {
                                            #[cfg(not(no_global_oom_handling))]
    #[unstable(feature = "clone_from_ref", issue = "149075")]
    pub fn clone_from_ref(value: &T) -> Arc<T> {
        Arc::clone_from_ref_in(value, Global)
    }

                                                    #[unstable(feature = "clone_from_ref", issue = "149075")]
        pub fn try_clone_from_ref(value: &T) -> Result<Arc<T>, AllocError> {
        Arc::try_clone_from_ref_in(value, Global)
    }
}

impl<T: ?Sized + CloneToUninit, A: Allocator> Arc<T, A> {
                                                    #[cfg(not(no_global_oom_handling))]
    #[unstable(feature = "clone_from_ref", issue = "149075")]
        pub fn clone_from_ref_in(value: &T, alloc: A) -> Arc<T, A> {
                let mut in_progress: UniqueArcUninit<T, A> = UniqueArcUninit::new(value, alloc);

                let initialized_clone = unsafe {
                        value.clone_to_uninit(in_progress.data_ptr().cast());
                        in_progress.into_arc()
        };

        initialized_clone
    }

                                                        #[unstable(feature = "clone_from_ref", issue = "149075")]
        pub fn try_clone_from_ref_in(value: &T, alloc: A) -> Result<Arc<T, A>, AllocError> {
                let mut in_progress: UniqueArcUninit<T, A> = UniqueArcUninit::try_new(value, alloc)?;

                let initialized_clone = unsafe {
                        value.clone_to_uninit(in_progress.data_ptr().cast());
                        in_progress.into_arc()
        };

        Ok(initialized_clone)
    }
}

impl<T, A: Allocator> Arc<[mem::MaybeUninit<T>], A> {
                                                                                                                        #[stable(feature = "new_uninit", since = "1.82.0")]
    #[must_use = "`self` will be dropped if the result is not used"]
    #[inline]
    pub unsafe fn assume_init(self) -> Arc<[T], A> {
        let (ptr, alloc) = Arc::into_inner_with_allocator(self);
        unsafe { Arc::from_ptr_in(ptr.as_ptr() as _, alloc) }
    }
}

impl<T: ?Sized> Arc<T> {
                                                                                                                                                                                                                                                                    #[inline]
    #[stable(feature = "rc_raw", since = "1.17.0")]
    pub unsafe fn from_raw(ptr: *const T) -> Self {
        unsafe { Arc::from_raw_in(ptr, Global) }
    }

                                                                    #[must_use = "losing the pointer will leak memory"]
    #[stable(feature = "rc_raw", since = "1.17.0")]
    #[rustc_never_returns_null_ptr]
    pub fn into_raw(this: Self) -> *const T {
        let this = ManuallyDrop::new(this);
        Self::as_ptr(&*this)
    }

                                                                                                                                    #[inline]
    #[stable(feature = "arc_mutate_strong_count", since = "1.51.0")]
    pub unsafe fn increment_strong_count(ptr: *const T) {
        unsafe { Arc::increment_strong_count_in(ptr, Global) }
    }

                                                                                                                                            #[inline]
    #[stable(feature = "arc_mutate_strong_count", since = "1.51.0")]
    pub unsafe fn decrement_strong_count(ptr: *const T) {
        unsafe { Arc::decrement_strong_count_in(ptr, Global) }
    }
}


impl<T: ?Sized> Arc<T> {
                        #[cfg(not(no_global_oom_handling))]
    unsafe fn allocate_for_layout(
        value_layout: Layout,
        allocate: impl FnOnce(Layout) -> Result<NonNull<[u8]>, AllocError>,
        mem_to_arcinner: impl FnOnce(*mut u8) -> *mut ArcInner<T>,
    ) -> *mut ArcInner<T> {
        let layout = arcinner_layout_for_value_layout(value_layout);

        let ptr = allocate(layout).unwrap_or_else(|_| handle_alloc_error(layout));

        unsafe { Self::initialize_arcinner(ptr, layout, mem_to_arcinner) }
    }

                            unsafe fn try_allocate_for_layout(
        value_layout: Layout,
        allocate: impl FnOnce(Layout) -> Result<NonNull<[u8]>, AllocError>,
        mem_to_arcinner: impl FnOnce(*mut u8) -> *mut ArcInner<T>,
    ) -> Result<*mut ArcInner<T>, AllocError> {
        let layout = arcinner_layout_for_value_layout(value_layout);

        let ptr = allocate(layout)?;

        let inner = unsafe { Self::initialize_arcinner(ptr, layout, mem_to_arcinner) };

        Ok(inner)
    }

    unsafe fn initialize_arcinner(
        ptr: NonNull<[u8]>,
        layout: Layout,
        mem_to_arcinner: impl FnOnce(*mut u8) -> *mut ArcInner<T>,
    ) -> *mut ArcInner<T> {
        let inner = mem_to_arcinner(ptr.as_non_null_ptr().as_ptr());
        debug_assert_eq!(unsafe { Layout::for_value_raw(inner) }, layout);

        unsafe {
            (&raw mut (*inner).strong).write(atomic::AtomicUsize::new(1));
            (&raw mut (*inner).weak).write(atomic::AtomicUsize::new(1));
        }

        inner
    }
}

impl<T: ?Sized, A: Allocator> Arc<T, A> {
        #[inline]
    #[cfg(not(no_global_oom_handling))]
    unsafe fn allocate_for_ptr_in(ptr: *const T, alloc: &A) -> *mut ArcInner<T> {
                unsafe {
            Arc::allocate_for_layout(
                Layout::for_value_raw(ptr),
                |layout| alloc.allocate(layout),
                |mem| mem.with_metadata_of(ptr as *const ArcInner<T>),
            )
        }
    }

    #[cfg(not(no_global_oom_handling))]
    fn from_box_in(src: Box<T, A>) -> Arc<T, A> {
        unsafe {
            let value_size = size_of_val(&*src);
            let ptr = Self::allocate_for_ptr_in(&*src, Box::allocator(&src));

                        ptr::copy_nonoverlapping(
                (&raw const *src) as *const u8,
                (&raw mut (*ptr).data) as *mut u8,
                value_size,
            );

                        let (bptr, alloc) = Box::into_raw_with_allocator(src);
            let src = Box::from_raw_in(bptr as *mut mem::ManuallyDrop<T>, alloc.by_ref());
            drop(src);

            Self::from_ptr_in(ptr, alloc)
        }
    }
}

impl<T> Arc<[T]> {
        #[cfg(not(no_global_oom_handling))]
    unsafe fn allocate_for_slice(len: usize) -> *mut ArcInner<[T]> {
        unsafe {
            Self::allocate_for_layout(
                Layout::array::<T>(len).unwrap(),
                |layout| Global.allocate(layout),
                |mem| ptr::slice_from_raw_parts_mut(mem.cast::<T>(), len) as *mut ArcInner<[T]>,
            )
        }
    }

                    #[cfg(not(no_global_oom_handling))]
    unsafe fn copy_from_slice(v: &[T]) -> Arc<[T]> {
        unsafe {
            let ptr = Self::allocate_for_slice(v.len());

            ptr::copy_nonoverlapping(v.as_ptr(), (&raw mut (*ptr).data) as *mut T, v.len());

            Self::from_ptr(ptr)
        }
    }

                #[cfg(not(no_global_oom_handling))]
    unsafe fn from_iter_exact(iter: impl Iterator<Item = T>, len: usize) -> Arc<[T]> {

        unsafe {
            let ptr = Self::allocate_for_slice(len);

            let mem = ptr as *mut _ as *mut u8;
            let layout = Layout::for_value_raw(ptr);

                        let elems = (&raw mut (*ptr).data) as *mut T;

            let mut guard = Guard { mem: NonNull::new_unchecked(mem), elems, layout, n_elems: 0 };

            for (i, item) in iter.enumerate() {
                ptr::write(elems.add(i), item);
                guard.n_elems += 1;
            }

                        mem::forget(guard);

            Self::from_ptr(ptr)
        }
    }
}

impl<T, A: Allocator> Arc<[T], A> {
        #[inline]
    #[cfg(not(no_global_oom_handling))]
    unsafe fn allocate_for_slice_in(len: usize, alloc: &A) -> *mut ArcInner<[T]> {
        unsafe {
            Arc::allocate_for_layout(
                Layout::array::<T>(len).unwrap(),
                |layout| alloc.allocate(layout),
                |mem| ptr::slice_from_raw_parts_mut(mem.cast::<T>(), len) as *mut ArcInner<[T]>,
            )
        }
    }
}

#[cfg(not(no_global_oom_handling))]
trait ArcFromSlice<T> {
    fn from_slice(slice: &[T]) -> Self;
}

#[cfg(not(no_global_oom_handling))]
impl<T: Clone> ArcFromSlice<T> for Arc<[T]> {
    #[inline]
    default fn from_slice(v: &[T]) -> Self {
        unsafe { Self::from_iter_exact(v.iter().cloned(), v.len()) }
    }
}

#[cfg(not(no_global_oom_handling))]
impl<T: TrivialClone> ArcFromSlice<T> for Arc<[T]> {
    #[inline]
    fn from_slice(v: &[T]) -> Self {
                        unsafe { Arc::copy_from_slice(v) }
    }
}


#[cfg(not(no_global_oom_handling))]
impl<T: ?Sized + CloneToUninit, A: Allocator + Clone> Arc<T, A> {
                                                                                                                                                                                                            #[inline]
    #[stable(feature = "arc_unique", since = "1.4.0")]
    pub fn make_mut(this: &mut Self) -> &mut T {
        let size_of_val = size_of_val::<T>(&**this);

                                                                        if this.inner().strong.compare_exchange(1, 0, Acquire, Relaxed).is_err() {
                        *this = Arc::clone_from_ref_in(&**this, this.alloc.clone());
        } else if this.inner().weak.load(Relaxed) != 1 {
                                    
                                    
                                    
                                    let _weak = Weak { ptr: this.ptr, alloc: this.alloc.clone() };

                                                            let mut in_progress: UniqueArcUninit<T, A> =
                UniqueArcUninit::new(&**this, this.alloc.clone());
            unsafe {
                                                                ptr::copy_nonoverlapping(
                    ptr::from_ref(&**this).cast::<u8>(),
                    in_progress.data_ptr().cast::<u8>(),
                    size_of_val,
                );

                ptr::write(this, in_progress.into_arc());
            }
        } else {
                                    this.inner().strong.store(1, Release);
        }

                        unsafe { Self::get_mut_unchecked(this) }
    }
}

impl<T: Clone, A: Allocator> Arc<T, A> {
                                                                                                                    #[inline]
    #[stable(feature = "arc_unwrap_or_clone", since = "1.76.0")]
    pub fn unwrap_or_clone(this: Self) -> T {
        Arc::try_unwrap(this).unwrap_or_else(|arc| (*arc).clone())
    }
}

impl<T: ?Sized, A: Allocator> Arc<T, A> {
                                                                                                    #[inline]
    #[stable(feature = "arc_unique", since = "1.4.0")]
    pub fn get_mut(this: &mut Self) -> Option<&mut T> {
        if Self::is_unique(this) {
                                                                        unsafe { Some(Arc::get_mut_unchecked(this)) }
        } else {
            None
        }
    }

                                                                                                                                                                                                                                                    #[inline]
    #[unstable(feature = "get_mut_unchecked", issue = "63292")]
    pub unsafe fn get_mut_unchecked(this: &mut Self) -> &mut T {
                        unsafe { &mut (*this.ptr.as_ptr()).data }
    }

                                                                                                                                                                                                                                #[inline]
    #[unstable(feature = "arc_is_unique", issue = "138938")]
    pub fn is_unique(this: &Self) -> bool {
                                                                if this.inner().weak.compare_exchange(1, usize::MAX, Acquire, Relaxed).is_ok() {
                                                let unique = this.inner().strong.load(Acquire) == 1;

                                                this.inner().weak.store(1, Release);             unique
        } else {
            false
        }
    }
}

impl<A: Allocator> Arc<dyn Any + Send + Sync, A> {
                                                                            #[inline]
    #[stable(feature = "rc_downcast", since = "1.29.0")]
    pub fn downcast<T>(self) -> Result<Arc<T, A>, Self>
    where
        T: Any + Send + Sync,
    {
        if (*self).is::<T>() {
            unsafe {
                let (ptr, alloc) = Arc::into_inner_with_allocator(self);
                Ok(Arc::from_inner_in(ptr.cast(), alloc))
            }
        } else {
            Err(self)
        }
    }

                                                                                                            #[inline]
    #[unstable(feature = "downcast_unchecked", issue = "90850")]
    pub unsafe fn downcast_unchecked<T>(self) -> Arc<T, A>
    where
        T: Any + Send + Sync,
    {
        unsafe {
            let (ptr, alloc) = Arc::into_inner_with_allocator(self);
            Arc::from_inner_in(ptr.cast(), alloc)
        }
    }
}

impl<T> Weak<T> {
                                                        #[inline]
    #[stable(feature = "downgraded_weak", since = "1.10.0")]
    #[rustc_const_stable(feature = "const_weak_new", since = "1.73.0")]
    #[must_use]
    pub const fn new() -> Weak<T> {
        Weak { ptr: NonNull::without_provenance(NonZeroUsize::MAX), alloc: Global }
    }
}


struct WeakInner<'a> {
    weak: &'a Atomic<usize>,
    strong: &'a Atomic<usize>,
}

impl<T: ?Sized> Weak<T> {
                                                                                                                                                                        #[inline]
    #[stable(feature = "weak_into_raw", since = "1.45.0")]
    pub unsafe fn from_raw(ptr: *const T) -> Self {
        unsafe { Weak::from_raw_in(ptr, Global) }
    }

                                                                                                                #[must_use = "losing the pointer will leak memory"]
    #[stable(feature = "weak_into_raw", since = "1.45.0")]
    pub fn into_raw(self) -> *const T {
        ManuallyDrop::new(self).as_ptr()
    }
}

impl<T: ?Sized, A: Allocator> Weak<T, A> {
        #[inline]
    #[unstable(feature = "allocator_api", issue = "32838")]
    pub fn allocator(&self) -> &A {
        &self.alloc
    }

                                                                                                        #[must_use]
    #[stable(feature = "weak_into_raw", since = "1.45.0")]
    pub fn as_ptr(&self) -> *const T {
        let ptr: *mut ArcInner<T> = NonNull::as_ptr(self.ptr);

        if is_dangling(ptr) {
                                    ptr as *const T
        } else {
                                                unsafe { &raw mut (*ptr).data }
        }
    }

                                                                                                                        #[must_use = "losing the pointer will leak memory"]
    #[unstable(feature = "allocator_api", issue = "32838")]
    pub fn into_raw_with_allocator(self) -> (*const T, A) {
        let this = mem::ManuallyDrop::new(self);
        let result = this.as_ptr();
                let alloc = unsafe { ptr::read(&this.alloc) };
        (result, alloc)
    }

                                                                                                                                                                            #[inline]
    #[unstable(feature = "allocator_api", issue = "32838")]
    pub unsafe fn from_raw_in(ptr: *const T, alloc: A) -> Self {
        
        let ptr = if is_dangling(ptr) {
                        ptr as *mut ArcInner<T>
        } else {
                                    let offset = unsafe { data_offset(ptr) };
                                    unsafe { ptr.byte_sub(offset) as *mut ArcInner<T> }
        };

                Weak { ptr: unsafe { NonNull::new_unchecked(ptr) }, alloc }
    }
}


#[unstable(feature = "ergonomic_clones", issue = "132290")]
impl<T: ?Sized, A: Allocator + Clone> UseCloned for Weak<T, A> {}



#[stable(feature = "rust1", since = "1.0.0")]
trait ArcEqIdent<T: ?Sized + PartialEq, A: Allocator> {
    fn eq(&self, other: &Arc<T, A>) -> bool;
    fn ne(&self, other: &Arc<T, A>) -> bool;
}

#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized + PartialEq, A: Allocator> ArcEqIdent<T, A> for Arc<T, A> {
    #[inline]
    default fn eq(&self, other: &Arc<T, A>) -> bool {
        **self == **other
    }
    #[inline]
    default fn ne(&self, other: &Arc<T, A>) -> bool {
        **self != **other
    }
}

#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized + crate::rc::MarkerEq, A: Allocator> ArcEqIdent<T, A> for Arc<T, A> {
    #[inline]
    fn eq(&self, other: &Arc<T, A>) -> bool {
        ptr::eq(self.ptr.as_ptr(), other.ptr.as_ptr()) || **self == **other
    }

    #[inline]
    fn ne(&self, other: &Arc<T, A>) -> bool {
        !ptr::eq(self.ptr.as_ptr(), other.ptr.as_ptr()) && **self != **other
    }
}

#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized + PartialEq, A: Allocator> PartialEq for Arc<T, A> {
                                                                        #[inline]
    fn eq(&self, other: &Arc<T, A>) -> bool {
        ArcEqIdent::eq(self, other)
    }

                                                                    #[inline]
    fn ne(&self, other: &Arc<T, A>) -> bool {
        ArcEqIdent::ne(self, other)
    }
}

#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized + PartialOrd, A: Allocator> PartialOrd for Arc<T, A> {
                                                            fn partial_cmp(&self, other: &Arc<T, A>) -> Option<Ordering> {
        (**self).partial_cmp(&**other)
    }

                                                        fn lt(&self, other: &Arc<T, A>) -> bool {
        *(*self) < *(*other)
    }

                                                        fn le(&self, other: &Arc<T, A>) -> bool {
        *(*self) <= *(*other)
    }

                                                        fn gt(&self, other: &Arc<T, A>) -> bool {
        *(*self) > *(*other)
    }

                                                        fn ge(&self, other: &Arc<T, A>) -> bool {
        *(*self) >= *(*other)
    }
}
#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized + Ord, A: Allocator> Ord for Arc<T, A> {
                                                            fn cmp(&self, other: &Arc<T, A>) -> Ordering {
        (**self).cmp(&**other)
    }
}
#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized + Eq, A: Allocator> Eq for Arc<T, A> {}

#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized + fmt::Display, A: Allocator> fmt::Display for Arc<T, A> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(&**self, f)
    }
}

#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized + fmt::Debug, A: Allocator> fmt::Debug for Arc<T, A> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&**self, f)
    }
}

#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized, A: Allocator> fmt::Pointer for Arc<T, A> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Pointer::fmt(&(&raw const **self), f)
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "rust1", since = "1.0.0")]
impl<T: Default> Default for Arc<T> {
                                            fn default() -> Arc<T> {
        unsafe {
            Self::from_inner(
                Box::leak(Box::write(
                    Box::new_uninit(),
                    ArcInner {
                        strong: atomic::AtomicUsize::new(1),
                        weak: atomic::AtomicUsize::new(1),
                        data: T::default(),
                    },
                ))
                .into(),
            )
        }
    }
}

#[repr(C, align(16))]
struct SliceArcInnerForStatic {
    inner: ArcInner<[u8; 1]>,
}
#[cfg(not(no_global_oom_handling))]
const MAX_STATIC_INNER_SLICE_ALIGNMENT: usize = 16;

static STATIC_INNER_SLICE: SliceArcInnerForStatic = SliceArcInnerForStatic {
    inner: ArcInner {
        strong: atomic::AtomicUsize::new(1),
        weak: atomic::AtomicUsize::new(1),
        data: [0],
    },
};

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "more_rc_default_impls", since = "1.80.0")]
impl Default for Arc<str> {
                #[inline]
    fn default() -> Self {
        let arc: Arc<[u8]> = Default::default();
        debug_assert!(core::str::from_utf8(&*arc).is_ok());
        let (ptr, alloc) = Arc::into_inner_with_allocator(arc);
        unsafe { Arc::from_ptr_in(ptr.as_ptr() as *mut ArcInner<str>, alloc) }
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "more_rc_default_impls", since = "1.80.0")]
impl Default for Arc<core::ffi::CStr> {
                #[inline]
    fn default() -> Self {
        use core::ffi::CStr;
        let inner: NonNull<ArcInner<[u8]>> = NonNull::from(&STATIC_INNER_SLICE.inner);
        let inner: NonNull<ArcInner<CStr>> =
            NonNull::new(inner.as_ptr() as *mut ArcInner<CStr>).unwrap();
                let this: mem::ManuallyDrop<Arc<CStr>> =
            unsafe { mem::ManuallyDrop::new(Arc::from_inner(inner)) };
        (*this).clone()
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "more_rc_default_impls", since = "1.80.0")]
impl<T> Default for Arc<[T]> {
                #[inline]
    fn default() -> Self {
        if align_of::<T>() <= MAX_STATIC_INNER_SLICE_ALIGNMENT {
                                                            let inner: NonNull<SliceArcInnerForStatic> = NonNull::from(&STATIC_INNER_SLICE);
            let inner: NonNull<ArcInner<[T; 0]>> = inner.cast();
                        let this: mem::ManuallyDrop<Arc<[T; 0]>> =
                unsafe { mem::ManuallyDrop::new(Arc::from_inner(inner)) };
            return (*this).clone();
        }

                let arr: [T; 0] = [];
        Arc::from(arr)
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "pin_default_impls", since = "1.91.0")]
impl<T> Default for Pin<Arc<T>>
where
    T: ?Sized,
    Arc<T>: Default,
{
    #[inline]
    fn default() -> Self {
        unsafe { Pin::new_unchecked(Arc::<T>::default()) }
    }
}

#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized + Hash, A: Allocator> Hash for Arc<T, A> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        (**self).hash(state)
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "shared_from_array", since = "1.74.0")]
impl<T, const N: usize> From<[T; N]> for Arc<[T]> {
                                                    #[inline]
    fn from(v: [T; N]) -> Arc<[T]> {
        Arc::<[T; N]>::from(v)
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "shared_from_slice", since = "1.21.0")]
impl<T: Clone> From<&[T]> for Arc<[T]> {
                                            #[inline]
    fn from(v: &[T]) -> Arc<[T]> {
        <Self as ArcFromSlice<T>>::from_slice(v)
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "shared_from_mut_slice", since = "1.84.0")]
impl<T: Clone> From<&mut [T]> for Arc<[T]> {
                                                #[inline]
    fn from(v: &mut [T]) -> Arc<[T]> {
        Arc::from(&*v)
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "shared_from_slice", since = "1.21.0")]
impl From<&str> for Arc<str> {
                                        #[inline]
    fn from(v: &str) -> Arc<str> {
        let arc = Arc::<[u8]>::from(v.as_bytes());
        unsafe { Arc::from_raw(Arc::into_raw(arc) as *const str) }
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "shared_from_mut_slice", since = "1.84.0")]
impl From<&mut str> for Arc<str> {
                                                #[inline]
    fn from(v: &mut str) -> Arc<str> {
        Arc::from(&*v)
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "shared_from_slice", since = "1.21.0")]
impl From<String> for Arc<str> {
                                            #[inline]
    fn from(v: String) -> Arc<str> {
        Arc::from(&v[..])
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "shared_from_slice", since = "1.21.0")]
impl<T: ?Sized, A: Allocator> From<Box<T, A>> for Arc<T, A> {
                                            #[inline]
    fn from(v: Box<T, A>) -> Arc<T, A> {
        Arc::from_box_in(v)
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "shared_from_slice", since = "1.21.0")]
impl<T, A: Allocator + Clone> From<Vec<T, A>> for Arc<[T], A> {
                                            #[inline]
    fn from(v: Vec<T, A>) -> Arc<[T], A> {
        unsafe {
            let (vec_ptr, len, cap, alloc) = v.into_raw_parts_with_alloc();

            let rc_ptr = Self::allocate_for_slice_in(len, &alloc);
            ptr::copy_nonoverlapping(vec_ptr, (&raw mut (*rc_ptr).data) as *mut T, len);

                                    let _ = Vec::from_raw_parts_in(vec_ptr, 0, cap, &alloc);

            Self::from_ptr_in(rc_ptr, alloc)
        }
    }
}

#[stable(feature = "shared_from_cow", since = "1.45.0")]
impl<'a, B> From<Cow<'a, B>> for Arc<B>
where
    B: ToOwned + ?Sized,
    Arc<B>: From<&'a B> + From<B::Owned>,
{
                                                    #[inline]
    fn from(cow: Cow<'a, B>) -> Arc<B> {
        match cow {
            Cow::Borrowed(s) => Arc::from(s),
            Cow::Owned(s) => Arc::from(s),
        }
    }
}

#[stable(feature = "shared_from_str", since = "1.62.0")]
impl From<Arc<str>> for Arc<[u8]> {
                                            #[inline]
    fn from(rc: Arc<str>) -> Self {
                unsafe { Arc::from_raw(Arc::into_raw(rc) as *const [u8]) }
    }
}

#[stable(feature = "boxed_slice_try_from", since = "1.43.0")]
impl<T, A: Allocator, const N: usize> TryFrom<Arc<[T], A>> for Arc<[T; N], A> {
    type Error = Arc<[T], A>;

    fn try_from(boxed_slice: Arc<[T], A>) -> Result<Self, Self::Error> {
        if boxed_slice.len() == N {
            let (ptr, alloc) = Arc::into_inner_with_allocator(boxed_slice);
            Ok(unsafe { Arc::from_inner_in(ptr.cast(), alloc) })
        } else {
            Err(boxed_slice)
        }
    }
}

#[cfg(not(no_global_oom_handling))]
#[stable(feature = "shared_from_iter", since = "1.37.0")]
impl<T> FromIterator<T> for Arc<[T]> {
                                                                                                                                                            fn from_iter<I: IntoIterator<Item = T>>(iter: I) -> Self {
        ToArcSlice::to_arc_slice(iter.into_iter())
    }
}

#[cfg(not(no_global_oom_handling))]
trait ToArcSlice<T>: Iterator<Item = T> + Sized {
    fn to_arc_slice(self) -> Arc<[T]>;
}

#[cfg(not(no_global_oom_handling))]
impl<T, I: Iterator<Item = T>> ToArcSlice<T> for I {
    default fn to_arc_slice(self) -> Arc<[T]> {
        self.collect::<Vec<T>>().into()
    }
}

#[cfg(not(no_global_oom_handling))]
impl<T, I: iter::TrustedLen<Item = T>> ToArcSlice<T> for I {
    fn to_arc_slice(self) -> Arc<[T]> {
                let (low, high) = self.size_hint();
        if let Some(high) = high {
            debug_assert_eq!(
                low,
                high,
                "TrustedLen iterator's size hint is not exact: {:?}",
                (low, high)
            );

            unsafe {
                                Arc::from_iter_exact(self, low)
            }
        } else {
                                                            panic!("capacity overflow");
        }
    }
}

#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized, A: Allocator> borrow::Borrow<T> for Arc<T, A> {
    fn borrow(&self) -> &T {
        &**self
    }
}

#[stable(since = "1.5.0", feature = "smart_ptr_as_ref")]
impl<T: ?Sized, A: Allocator> AsRef<T> for Arc<T, A> {
    fn as_ref(&self) -> &T {
        &**self
    }
}

#[stable(feature = "pin", since = "1.33.0")]
impl<T: ?Sized, A: Allocator> Unpin for Arc<T, A> {}

unsafe fn data_offset<T: ?Sized>(ptr: *const T) -> usize {
                            unsafe { data_offset_alignment(Alignment::of_val_raw(ptr)) }
}

#[inline]
fn data_offset_alignment(alignment: Alignment) -> usize {
    let layout = Layout::new::<ArcInner<()>>();
    layout.size() + layout.padding_needed_for(alignment)
}

struct UniqueArcUninit<T: ?Sized, A: Allocator> {
    ptr: NonNull<ArcInner<T>>,
    layout_for_value: Layout,
    alloc: Option<A>,
}

impl<T: ?Sized, A: Allocator> UniqueArcUninit<T, A> {
        #[cfg(not(no_global_oom_handling))]
    fn new(for_value: &T, alloc: A) -> UniqueArcUninit<T, A> {
        let layout = Layout::for_value(for_value);
        let ptr = unsafe {
            Arc::allocate_for_layout(
                layout,
                |layout_for_arcinner| alloc.allocate(layout_for_arcinner),
                |mem| mem.with_metadata_of(ptr::from_ref(for_value) as *const ArcInner<T>),
            )
        };
        Self { ptr: NonNull::new(ptr).unwrap(), layout_for_value: layout, alloc: Some(alloc) }
    }

            fn try_new(for_value: &T, alloc: A) -> Result<UniqueArcUninit<T, A>, AllocError> {
        let layout = Layout::for_value(for_value);
        let ptr = unsafe {
            Arc::try_allocate_for_layout(
                layout,
                |layout_for_arcinner| alloc.allocate(layout_for_arcinner),
                |mem| mem.with_metadata_of(ptr::from_ref(for_value) as *const ArcInner<T>),
            )?
        };
        Ok(Self { ptr: NonNull::new(ptr).unwrap(), layout_for_value: layout, alloc: Some(alloc) })
    }

        fn data_ptr(&mut self) -> *mut T {
        let offset = data_offset_alignment(self.layout_for_value.alignment());
        unsafe { self.ptr.as_ptr().byte_add(offset) as *mut T }
    }

                        unsafe fn into_arc(self) -> Arc<T, A> {
        let mut this = ManuallyDrop::new(self);
        let ptr = this.ptr.as_ptr();
        let alloc = this.alloc.take().unwrap();

                        unsafe { Arc::from_ptr_in(ptr, alloc) }
    }
}


#[stable(feature = "arc_error", since = "1.52.0")]
impl<T: core::error::Error + ?Sized> core::error::Error for Arc<T> {
    #[allow(deprecated)]
    fn cause(&self) -> Option<&dyn core::error::Error> {
        core::error::Error::cause(&**self)
    }

    fn source(&self) -> Option<&(dyn core::error::Error + 'static)> {
        core::error::Error::source(&**self)
    }

    fn provide<'a>(&'a self, req: &mut core::error::Request<'a>) {
        core::error::Error::provide(&**self, req);
    }
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
pub struct UniqueArc<
    T: ?Sized,
    #[unstable(feature = "allocator_api", issue = "32838")] A: Allocator = Global,
> {
    ptr: NonNull<ArcInner<T>>,
        _marker: PhantomData<ArcInner<T>>,
            _marker2: PhantomData<*mut T>,
    alloc: A,
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
unsafe impl<T: ?Sized + Sync + Send, A: Allocator + Send> Send for UniqueArc<T, A> {}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
unsafe impl<T: ?Sized + Sync + Send, A: Allocator + Sync> Sync for UniqueArc<T, A> {}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized + Unsize<U>, U: ?Sized, A: Allocator> CoerceUnsized<UniqueArc<U, A>>
    for UniqueArc<T, A>
{
}

#[unstable(feature = "dispatch_from_dyn", issue = "none")]
impl<T: ?Sized + Unsize<U>, U: ?Sized> DispatchFromDyn<UniqueArc<U>> for UniqueArc<T> {}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized + fmt::Display, A: Allocator> fmt::Display for UniqueArc<T, A> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(&**self, f)
    }
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized + fmt::Debug, A: Allocator> fmt::Debug for UniqueArc<T, A> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&**self, f)
    }
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized, A: Allocator> fmt::Pointer for UniqueArc<T, A> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Pointer::fmt(&(&raw const **self), f)
    }
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized, A: Allocator> borrow::Borrow<T> for UniqueArc<T, A> {
    fn borrow(&self) -> &T {
        &**self
    }
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized, A: Allocator> borrow::BorrowMut<T> for UniqueArc<T, A> {
    fn borrow_mut(&mut self) -> &mut T {
        &mut **self
    }
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized, A: Allocator> AsRef<T> for UniqueArc<T, A> {
    fn as_ref(&self) -> &T {
        &**self
    }
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized, A: Allocator> AsMut<T> for UniqueArc<T, A> {
    fn as_mut(&mut self) -> &mut T {
        &mut **self
    }
}

#[cfg(not(no_global_oom_handling))]
#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T> From<T> for UniqueArc<T> {
    #[inline(always)]
    fn from(value: T) -> Self {
        Self::new(value)
    }
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized, A: Allocator> Unpin for UniqueArc<T, A> {}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized + PartialEq, A: Allocator> PartialEq for UniqueArc<T, A> {
                                                            #[inline]
    fn eq(&self, other: &Self) -> bool {
        PartialEq::eq(&**self, &**other)
    }
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized + PartialOrd, A: Allocator> PartialOrd for UniqueArc<T, A> {
                                                                #[inline(always)]
    fn partial_cmp(&self, other: &UniqueArc<T, A>) -> Option<Ordering> {
        (**self).partial_cmp(&**other)
    }

                                                            #[inline(always)]
    fn lt(&self, other: &UniqueArc<T, A>) -> bool {
        **self < **other
    }

                                                            #[inline(always)]
    fn le(&self, other: &UniqueArc<T, A>) -> bool {
        **self <= **other
    }

                                                            #[inline(always)]
    fn gt(&self, other: &UniqueArc<T, A>) -> bool {
        **self > **other
    }

                                                            #[inline(always)]
    fn ge(&self, other: &UniqueArc<T, A>) -> bool {
        **self >= **other
    }
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized + Ord, A: Allocator> Ord for UniqueArc<T, A> {
                                                                #[inline]
    fn cmp(&self, other: &UniqueArc<T, A>) -> Ordering {
        (**self).cmp(&**other)
    }
}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized + Eq, A: Allocator> Eq for UniqueArc<T, A> {}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
impl<T: ?Sized + Hash, A: Allocator> Hash for UniqueArc<T, A> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        (**self).hash(state);
    }
}

impl<T: ?Sized> UniqueArc<T> {
    #[cfg(not(no_global_oom_handling))]
    unsafe fn from_raw(ptr: *const T) -> Self {
        let offset = unsafe { data_offset(ptr) };

                let rc_ptr = unsafe { ptr.byte_sub(offset) as *mut ArcInner<T> };

        Self {
            ptr: unsafe { NonNull::new_unchecked(rc_ptr) },
            _marker: PhantomData,
            _marker2: PhantomData,
            alloc: Global,
        }
    }

    #[cfg(not(no_global_oom_handling))]
    fn into_raw(this: Self) -> *const T {
        let this = ManuallyDrop::new(this);
        Self::as_ptr(&*this)
    }
}

impl<T: ?Sized, A: Allocator> UniqueArc<T, A> {
                                #[unstable(feature = "unique_rc_arc", issue = "112566")]
    #[must_use]
    pub fn into_arc(this: Self) -> Arc<T, A> {
        let this = ManuallyDrop::new(this);
        let alloc: A = unsafe { ptr::read(&this.alloc) };

            unsafe { (*this.ptr.as_ptr()).strong.store(1, Release);
            Arc::from_inner_in(this.ptr, alloc)
        }
    }

    #[cfg(not(no_global_oom_handling))]
    fn weak_count(this: &Self) -> usize {
        this.inner().weak.load(Acquire) - 1
    }

    #[cfg(not(no_global_oom_handling))]
    fn inner(&self) -> &ArcInner<T> {
                unsafe { self.ptr.as_ref() }
    }

    #[cfg(not(no_global_oom_handling))]
    fn as_ptr(this: &Self) -> *const T {
        let ptr: *mut ArcInner<T> = NonNull::as_ptr(this.ptr);

                                unsafe { &raw mut (*ptr).data }
    }

    #[inline]
    #[cfg(not(no_global_oom_handling))]
    fn into_inner_with_allocator(this: Self) -> (NonNull<ArcInner<T>>, A) {
        let this = mem::ManuallyDrop::new(this);
        (this.ptr, unsafe { ptr::read(&this.alloc) })
    }

    #[inline]
    #[cfg(not(no_global_oom_handling))]
    unsafe fn from_inner_in(ptr: NonNull<ArcInner<T>>, alloc: A) -> Self {
        Self { ptr, _marker: PhantomData, _marker2: PhantomData, alloc }
    }
}

impl<T: ?Sized, A: Allocator + Clone> UniqueArc<T, A> {
                    #[unstable(feature = "unique_rc_arc", issue = "112566")]
    #[must_use]
    pub fn downgrade(this: &Self) -> Weak<T, A> {
                                                                                let old_size = unsafe { (*this.ptr.as_ptr()).weak.fetch_add(1, Relaxed) };

                if old_size > MAX_REFCOUNT {
            abort();
        }

        Weak { ptr: this.ptr, alloc: this.alloc.clone() }
    }
}

#[cfg(not(no_global_oom_handling))]
impl<T, A: Allocator> UniqueArc<mem::MaybeUninit<T>, A> {
    unsafe fn assume_init(self) -> UniqueArc<T, A> {
        let (ptr, alloc) = UniqueArc::into_inner_with_allocator(self);
        unsafe { UniqueArc::from_inner_in(ptr.cast(), alloc) }
    }
}


#[unstable(feature = "pin_coerce_unsized_trait", issue = "150112")]
unsafe impl<T: ?Sized> PinCoerceUnsized for UniqueArc<T> {}

#[unstable(feature = "unique_rc_arc", issue = "112566")]
unsafe impl<T: ?Sized, A: Allocator> DerefPure for UniqueArc<T, A> {}


#[unstable(feature = "allocator_api", issue = "32838")]
unsafe impl<T: ?Sized + Allocator, A: Allocator> Allocator for Arc<T, A> {
    #[inline]
    fn allocate(&self, layout: Layout) -> Result<NonNull<[u8]>, AllocError> {
        (**self).allocate(layout)
    }

    #[inline]
    fn allocate_zeroed(&self, layout: Layout) -> Result<NonNull<[u8]>, AllocError> {
        (**self).allocate_zeroed(layout)
    }

    #[inline]
    unsafe fn deallocate(&self, ptr: NonNull<u8>, layout: Layout) {
                unsafe { (**self).deallocate(ptr, layout) }
    }

    #[inline]
    unsafe fn grow(
        &self,
        ptr: NonNull<u8>,
        old_layout: Layout,
        new_layout: Layout,
    ) -> Result<NonNull<[u8]>, AllocError> {
                unsafe { (**self).grow(ptr, old_layout, new_layout) }
    }

    #[inline]
    unsafe fn grow_zeroed(
        &self,
        ptr: NonNull<u8>,
        old_layout: Layout,
        new_layout: Layout,
    ) -> Result<NonNull<[u8]>, AllocError> {
                unsafe { (**self).grow_zeroed(ptr, old_layout, new_layout) }
    }

    #[inline]
    unsafe fn shrink(
        &self,
        ptr: NonNull<u8>,
        old_layout: Layout,
        new_layout: Layout,
    ) -> Result<NonNull<[u8]>, AllocError> {
                unsafe { (**self).shrink(ptr, old_layout, new_layout) }
    }
}


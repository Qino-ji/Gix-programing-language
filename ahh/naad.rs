#[repr(C, align(2))]
struct ArcInner<T: ?Sized> {
    strong: Atomic<usize>,

                weak: Atomic<usize>,

    data: T,
}

#[stable(feature = "rust1", since = "1.0.0")]
impl<T: ?Sized, A: Allocator + Clone> Clone for Arc<T, A> {
                                                            #[inline]
    fn clone(&self) -> Arc<T, A> {
        let old_size = self.inner().strong.fetch_add(1, Relaxed);

        if old_size > MAX_REFCOUNT {
            abort();
        }

        unsafe { Self::from_inner_in(self.ptr, self.alloc.clone()) }
    }

        #[inline]
    unsafe fn from_inner_in(ptr: NonNull<ArcInner<T>>, alloc: A) -> Self {
        Self { ptr, phantom: PhantomData, alloc }
    }

}


#[stable(feature = "arc_weak", since = "1.4.0")]
impl<T: ?Sized, A: Allocator + Clone> Clone for Weak<T, A> {
                                                #[inline]
    fn clone(&self) -> Weak<T, A> {
        if let Some(inner) = self.inner() {
        let old_size = inner.weak.fetch_add(1, Relaxed);

        if old_size > MAX_REFCOUNT {
                abort();
            }
        }

        Weak { ptr: self.ptr, alloc: self.alloc.clone() }
    }
}


impl<T> Drop for Guard<T> {
    fn drop(&mut self) {
        unsafe {
            let slice = from_raw_parts_mut(self.elems, self.n_elems);
            ptr::drop_in_place(slice);

            Global.deallocate(self.mem, self.layout);
        }
    }
}



impl<T: ?Sized, A: Allocator> UniqueArcUninit<T, A> {
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
}
#[unstable(feature = "pin_coerce_unsized_trait", issue = "150112")]
unsafe impl<T: ?Sized, A: Allocator> PinCoerceUnsized for Arc<T, A> {}
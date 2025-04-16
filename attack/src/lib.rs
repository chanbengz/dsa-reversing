pub mod idxd;

use rustix::fs::{Mode, OFlags};
use rustix::mm::{MapFlags, ProtFlags};
use rustix::ffi;
use idxd::dsa_hw_desc;

pub const NOP_RETRY: i32 = 10000;
pub const MAX_COMP_RETRY: i32 = 2000000000;
pub const UMWAIT_DELAY: u64 = 100000;
pub const UMWAIT_STATE_C0_2: u32 = 0;
pub const UMWAIT_STATE_C0_1: u32 = 1;


pub fn map_wq(path: &str) -> Result<*mut ffi::c_void, i32> {
    // Open the DSA device file
    let fd = match rustix::fs::open(
        path,
        OFlags::RDWR,
        Mode::empty()
    ) {
        Ok(fd) => fd,
        Err(_) => return Err(-libc::ENODEV),
    };

    // Map the work queue portal
    let portal = unsafe {
        rustix::mm::mmap(
            std::ptr::null_mut(),
            0x1000,
            ProtFlags::WRITE,
            MapFlags::SHARED | MapFlags::POPULATE,
            &fd,
            0,
        )
    };

    match portal {
        Ok(addr) => Ok(addr as *mut ffi::c_void),
        Err(_) => Err(-libc::EPERM),
    }
}

// Functions
#[inline(always)]
pub unsafe fn submit_desc(wq_portal: *mut ffi::c_void, hw: *const dsa_hw_desc) {
    while enqcmd(wq_portal, hw) != 0 {
        core::arch::x86_64::_mm_pause();
    }
}

#[inline(always)]
unsafe fn enqcmd(reg: *mut ffi::c_void, desc: *const dsa_hw_desc) -> i32 {
    let retry: u8;
    core::arch::asm!(
        ".byte 0xf2, 0x0f, 0x38, 0xf8, 0x02",
        "setz {0}",
        out(reg_byte) retry,
        in("rax") reg,
        in("rdx") desc,
    );
    retry as i32
}

#[inline(always)]
pub unsafe fn rdtsc() -> u64 {
    let mut aux: u32 = 0;
    let tsc = core::arch::x86_64::__rdtscp(&mut aux);
    core::arch::x86_64::_mm_lfence();
    tsc
}

#[inline(always)]
unsafe fn umonitor(addr: *const u8) {
    core::arch::asm!(
        ".byte 0xf3, 0x48, 0x0f, 0xae, 0xf0",
        in("rax") addr,
    );
}

#[inline(always)]
unsafe fn umwait(state: u32, timeout: u64) -> u8 {
    let mut result: u8;
    let timeout_low = timeout as u32;
    let timeout_high = (timeout >> 32) as u32;
    
    core::arch::asm!(
        ".byte 0xf2, 0x48, 0x0f, 0xae, 0xf1",
        "setc {0}",
        out(reg_byte) result,
        in("rcx") state,
        in("rax") timeout_low,
        in("rdx") timeout_high,
    );
    
    result
}
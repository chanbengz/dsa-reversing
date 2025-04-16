use dsa_attack::*;
use idxd::*;
use rustix::ffi;

fn main() {
    let wq_portal = map_wq("/dev/dsa0/wq0.0");
    match wq_portal {
        Ok(wq_portal) => {
            submit(wq_portal);
        },
        Err(_) => println!("Failed to map wq0.0"),
    };
}

#[inline(always)]
fn submit(_wq_portal : *mut ffi::c_void) {
    let mut desc = dsa_hw_desc::new();
    let _comp = dsa_completion_record::new();
    desc.set_opcode(DSA_OPCODE_COMPARE);
    unsafe {
        println!("desc address: {:p}", &desc as *const _);
    }
    // desc.completion_addr = (&comp as *const dsa_completion_record) as u64;
    // unsafe {
    //     submit_desc(wq_portal, &desc);
    // }
}
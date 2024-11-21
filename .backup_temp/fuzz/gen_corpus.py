#!/usr/bin/env python3
import os
import struct
import random
import argparse

def gen_vault_parser_corpus(out_dir, count=10):
    os.makedirs(out_dir, exist_ok=True)
    for i in range(count):
        # mode (0-4), sub_mode, payload...
        mode = i % 5
        sub_mode = random.randint(0, 255)
        
        with open(os.path.join(out_dir, f"seed_{i}.bin"), "wb") as f:
            f.write(bytes([mode, sub_mode]))
            
            if mode == 0:
                # TLV data
                f.write(bytes([0x30, 0x06, 0x02, 0x01, 0x2A, 0x01, 0x01, 0xFF]))
            elif mode == 1:
                # vault tree ops
                f.write(bytes([0x03])) # num nodes
                f.write(b"\x01\x11\x22\x33\x02\x44\x55\x66\x03\x77\x88\x99")
            elif mode == 2:
                # Alias
                f.write(os.urandom(16))
            elif mode == 3:
                # iterators
                f.write(os.urandom(32))
            elif mode == 4:
                # TLV encode
                f.write(bytes([5]) + os.urandom(10))

def gen_kdf_runner_corpus(out_dir, count=10):
    os.makedirs(out_dir, exist_ok=True)
    for i in range(count):
        mode = i % 4
        sub_mode = random.randint(0, 255)
        
        with open(os.path.join(out_dir, f"seed_{i}.bin"), "wb") as f:
            f.write(bytes([mode, sub_mode, 0x00, 0x00]))
            
            if mode == 0:
                f.write(struct.pack("<III", 100, 32, 16) + os.urandom(64))
            elif mode == 1:
                f.write(os.urandom(64))
            elif mode == 2:
                f.write(bytes([random.choice([0, 1])]) + os.urandom(15))
            elif mode == 3:
                f.write(bytes([0x30, 0x08, 0x30, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x02]))

def gen_acl_merge_corpus(out_dir, count=10):
    os.makedirs(out_dir, exist_ok=True)
    for i in range(count):
        mode = i % 3
        with open(os.path.join(out_dir, f"seed_{i}.bin"), "wb") as f:
            f.write(bytes([mode, 0x00]))
            if mode == 0:
                f.write(bytes([5]) + os.urandom(20))
            elif mode == 1:
                f.write(bytes([3, 1, 2, 3, 2, 3, 4, 3, 4, 5]))
            elif mode == 2:
                f.write(bytes([1]) + os.urandom(32))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="fuzz/corpus")
    args = parser.parse_args()
    
    gen_vault_parser_corpus(os.path.join(args.out, "vault_parser_fuzzer"))
    gen_kdf_runner_corpus(os.path.join(args.out, "kdf_runner_fuzzer"))
    gen_acl_merge_corpus(os.path.join(args.out, "acl_merge_fuzzer"))
    print("Corpus generation complete.")

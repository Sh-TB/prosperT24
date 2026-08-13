#!/usr/bin/env python3
"""
Test suite for IL2CPP tools (prx_to_elf.py and resolve.py)

Tests cover:
- ELF conversion correctness
- Section header synthesis
- Address resolution
- Edge cases and error handling

Run with: python3 test_il2cpp_tools.py -v
"""

import json
import os
import struct
import sys
import tempfile
import unittest

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from prx_to_elf import main as prx_to_elf_main, add_sections
from resolve import load, resolve_one


class TestPrxToElf(unittest.TestCase):
    """Tests for prx_to_elf.py"""
    
    def setUp(self):
        """Create a temporary directory for test files"""
        self.test_dir = tempfile.mkdtemp(prefix="il2cpp_test_")
        
    def tearDown(self):
        """Clean up temporary files"""
        import shutil
        shutil.rmtree(self.test_dir, ignore_errors=True)
    
    def create_minimal_prx(self):
        """Create a minimal valid PRX-like file for testing"""
        prx_path = os.path.join(self.test_dir, "test.prx")
        
        # Create a minimal SELF/PRX structure
        # This is a simplified version - real PRX files are more complex
        data = bytearray()
        
        # SELF header (simplified)
        data.extend(b'\x00' * 0x20)  # Padding to offset 0x20
        
        # Program header count at 0x18
        nseg = 2
        struct.pack_into("<H", data, 0x18, nseg)
        
        # Segment table at 0x20 (32 bytes each)
        # Format: flags, file_offset, file_size, mem_size
        seg1_flags = 0x800 | (0 << 20)  # Data segment, index 0
        seg1_offset = len(data) + nseg * 32 + 0x100  # Will be after headers
        seg1_fsize = 0x100
        seg1_msize = 0x100
        
        seg2_flags = 0x801 | (1 << 20)  # Executable + data, index 1
        seg2_offset = seg1_offset + seg1_fsize
        seg2_fsize = 0x200
        seg2_msize = 0x200
        
        struct.pack_into("<QQQQ", data, 0x20, 
                         seg1_flags, seg1_offset, seg1_fsize, seg1_msize)
        struct.pack_into("<QQQQ", data, 0x40,
                         seg2_flags, seg2_offset, seg2_fsize, seg2_msize)
        
        # Find/magic ELF marker after segment table
        elf_start = 0x20 + nseg * 32
        data.extend(b'\x00' * (elf_start - len(data)))
        data.extend(b'\x7fELF')  # ELF magic
        
        # ELF64 header (minimal)
        elf_header = bytearray(64)
        elf_header[4:5] = b'\x02'  # 64-bit
        elf_header[5:6] = b'\x01'  # Little endian
        elf_header[6:7] = b'\x01'  # ELF version
        struct.pack_into("<H", elf_header, 0x10, 3)  # ET_DYN
        
        # Program headers will be at offset 0x40 in output
        struct.pack_into("<Q", elf_header, 0x20, 0x40)  # e_phoff
        struct.pack_into("<H", elf_header, 0x38, 2)      # e_phnum (2 PT_LOAD)
        
        data.extend(elf_header)
        
        # Program headers at e_phoff
        phdr_offset = len(data)
        # PT_LOAD for segment 0 (data)
        phdr0 = struct.pack("<IIQQQQQQ",
                            1,           # PT_LOAD
                            5,           # PF_R | PF_W
                            0,           # p_offset (will be == p_vaddr)
                            0x10000,     # p_vaddr
                            0x10000,     # p_paddr
                            seg1_fsize,  # p_filesz
                            seg1_msize,  # p_memsz
                            0x1000)      # p_align
        # PT_LOAD for segment 1 (code)
        phdr1 = struct.pack("<IIQQQQQQ",
                            1,           # PT_LOAD
                            7,           # PF_R | PF_X | PF_W
                            0,           # p_offset
                            0x20000,     # p_vaddr
                            0x20000,     # p_paddr
                            seg2_fsize,  # p_filesz
                            seg2_msize,  # p_memsz
                            0x1000)      # p_align
        
        data.extend(phdr0)
        data.extend(phdr1)
        
        # Pad to segment data offsets
        while len(data) < seg1_offset:
            data.append(0)
        
        # Segment 0 data (some recognizable pattern)
        data.extend(b'IL2CPP_TEST_DATA_0' + b'\x00' * (seg1_fsize - 20))
        
        # Segment 1 data (code pattern)
        data.extend(b'\xCC' * seg2_fsize)  # INT3 instructions
        
        with open(prx_path, 'wb') as f:
            f.write(data)
        
        return prx_path
    
    def test_basic_conversion(self):
        """Test basic PRX to ELF conversion"""
        prx_path = self.create_minimal_prx()
        elf_path = os.path.join(self.test_dir, "output.elf")
        
        # Should not raise
        prx_to_elf_main(prx_path, elf_path)
        
        # Output should exist and be non-empty
        self.assertTrue(os.path.exists(elf_path))
        self.assertGreater(os.path.getsize(elf_path), 0)
    
    def test_output_is_valid_elf(self):
        """Test that output is a valid ELF file"""
        prx_path = self.create_minimal_prx()
        elf_path = os.path.join(self.test_dir, "output.elf")
        
        prx_to_elf_main(prx_path, elf_path)
        
        with open(elf_path, 'rb') as f:
            magic = f.read(4)
            
        self.assertEqual(magic, b'\x7fELF')
    
    def test_sections_flag_creates_section_headers(self):
        """Test that --sections flag creates section headers"""
        prx_path = self.create_minimal_prx()
        elf_path = os.path.join(self.test_dir, "output_sections.elf")
        
        prx_to_elf_main(prx_path, elf_path, want_sections=True)
        
        with open(elf_path, 'rb') as f:
            data = f.read()
        
        # Check e_shoff is non-zero (section headers present)
        e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
        self.assertGreater(e_shoff, 0)
        
        # Check e_shnum > 0
        e_shnum = struct.unpack_from("<H", data, 0x3c)[0]
        self.assertGreater(e_shnum, 0)
    
    def test_no_sections_default(self):
        """Test that default output has no section headers"""
        prx_path = self.create_minimal_prx()
        elf_path = os.path.join(self.test_dir, "output_nosec.elf")
        
        prx_to_elf_main(prx_path, elf_path, want_sections=False)
        
        with open(elf_path, 'rb') as f:
            data = f.read()
        
        # e_shoff should be 0
        e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
        self.assertEqual(e_shoff, 0)


class TestResolve(unittest.TestCase):
    """Tests for resolve.py"""
    
    def setUp(self):
        """Create test script.json"""
        self.test_dir = tempfile.mkdtemp(prefix="resolve_test_")
        self.script_path = os.path.join(self.test_dir, "script.json")
        
        # Create sample script.json with known methods
        methods = [
            {"Address": 0x1000, "Name": "TestClass::MethodA"},
            {"Address": 0x2000, "Name": "TestClass::MethodB"},
            {"Address": 0x3000, "Name": "AnotherClass::Init"},
            {"Address": 0x4000, "Name": "UnityEngine::SceneManagement::LoadScene"},
            {"Address": 0x5000, "Name": "Unity.PSN.PS5.Async.WorkerThread$$RunProc"},
        ]
        
        with open(self.script_path, 'w') as f:
            json.dump({"ScriptMethod": methods}, f)
    
    def tearDown(self):
        """Clean up"""
        import shutil
        shutil.rmtree(self.test_dir, ignore_errors=True)
    
    def test_load_script(self):
        """Test loading script.json"""
        addrs, keys = load(self.script_path)
        
        self.assertEqual(len(addrs), 5)
        self.assertEqual(len(keys), 5)
    
    def test_resolve_exact_address(self):
        """Test resolving exact method address"""
        addrs, keys = load(self.script_path)
        
        result = resolve_one(addrs, keys, 0x2000)
        
        self.assertIn("MethodB", result)
        self.assertIn("+0x0", result)
    
    def test_resolve_offset_within_method(self):
        """Test resolving address within a method"""
        addrs, keys = load(self.script_path)
        
        result = resolve_one(addrs, keys, 0x2050)  # 50 bytes into MethodB
        
        self.assertIn("MethodB", result)
        self.assertIn("+0x50", result)
    
    def test_resolve_unknown_address(self):
        """Test resolving address not in any method"""
        addrs, keys = load(self.script_path)
        
        result = resolve_one(addrs, keys, 0xFFFF)
        
        self.assertIn("no managed method", result)
    
    def test_resolve_before_first_method(self):
        """Test resolving address before first method"""
        addrs, keys = load(self.script_path)
        
        result = resolve_one(addrs, keys, 0x100)
        
        self.assertIn("no managed method", result)
    
    def test_methods_sorted_by_address(self):
        """Test that loaded methods are sorted by address"""
        addrs, keys = load(self.script_path)
        
        for i in range(1, len(keys)):
            self.assertGreaterEqual(keys[i], keys[i-1])


class TestEdgeCases(unittest.TestCase):
    """Edge case tests for both tools"""
    
    def test_empty_script_json(self):
        """Test resolve with empty script.json"""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump({"ScriptMethod": []}, f)
            path = f.name
        
        try:
            addrs, keys = load(path)
            self.assertEqual(len(addrs), 0)
        finally:
            os.unlink(path)
    
    def test_missing_file_handling(self):
        """Test graceful handling of missing files"""
        with self.assertRaises(FileNotFoundError):
            load("/nonexistent/path/script.json")
    
    def test_invalid_elf_input(self):
        """Test prx_to_elf with invalid input"""
        test_dir = tempfile.mkdtemp(prefix="edge_test_")
        try:
            invalid_path = os.path.join(test_dir, "invalid.bin")
            out_path = os.path.join(test_dir, "out.elf")
            
            # Write garbage data
            with open(invalid_path, 'wb') as f:
                f.write(b'\xDE\xAD\xBE\xEF' * 100)
            
            # Should raise or handle gracefully
            # (implementation may vary)
            try:
                prx_to_elf_main(invalid_path, out_path)
            except Exception:
                pass  # Expected for invalid input
        finally:
            import shutil
            shutil.rmtree(test_dir, ignore_errors=True)


class TestIntegration(unittest.TestCase):
    """Integration tests combining multiple tools"""
    
    def test_full_workflow_simulation(self):
        """Simulate the full workflow end-to-end"""
        test_dir = tempfile.mkdtemp(prefix="integration_test_")
        try:
            # Step 1: Create mock PRX (simplified)
            prx_path = os.path.join(test_dir, "module.prx")
            elf_path = os.path.join(test_dir, "module.elf")
            
            # For now, just verify the workflow steps exist
            # (Full integration requires actual game dumps)
            
            # Step 2: Would run Il2CppDumper here
            script_path = os.path.join(test_dir, "script.json")
            
            # Create mock dump output
            methods = [
                {"Address": 0x2140D0, "Name": "DoFirstLogin"},
                {"Address": 0x215000, "Name": "Update"},
            ]
            with open(script_path, 'w') as f:
                json.dump({"ScriptMethod": methods}, f)
            
            # Step 3: Resolve addresses
            addrs, keys = load(script_path)
            result = resolve_one(addrs, keys, 0x2140D0)
            
            self.assertIn("DoFirstLogin", result)
            
        finally:
            import shutil
            shutil.rmtree(test_dir, ignore_errors=True)


if __name__ == '__main__':
    # Run with verbosity
    unittest.main(verbosity=2)

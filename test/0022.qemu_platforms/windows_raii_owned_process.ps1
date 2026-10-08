# SOURCE ONLY test-owned job helper derivative. No product/runtime ABI change.
# Parent source: run_native_step_windows_product_vm.ps1; preserve old snapshot.
# New continuous WriteInputLine never closes the inherited private pipe.
# Test-only real Win32 owner; embedded bytes are covered by each runner SHA.
# CreateProcessW stays suspended until this unique non-inheritable job owns it.
# No process-name, CIM parent PID or unknown process-group kill is performed.
$script:currentDebugOwnedTypesInitialized = $false
function Initialize-CurrentDebugOwnedProcessTypes {
    if ($script:currentDebugOwnedTypesInitialized) { return }
    if ('UwvmWindowsDebugAcceptanceOwnedProcessR1' -as [type]) { throw 'fresh PowerShell host required for exact owned helper' }
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

public sealed class UwvmWindowsDebugAcceptanceOwnedProcessR1 : IDisposable {
    [StructLayout(LayoutKind.Sequential)] struct Startup {
        public uint cb; public IntPtr reserved, desktop, title;
        public uint x, y, width, height, xchars, ychars, fill, flags;
        public ushort show, reservedBytes;
        public IntPtr reservedData, input, output, error;
    }
    [StructLayout(LayoutKind.Sequential)] struct StartupEx {
        public Startup startup; public IntPtr attributes;
    }
    [StructLayout(LayoutKind.Sequential)] struct ProcessInfo {
        public IntPtr process, thread; public uint pid, tid;
    }
    [StructLayout(LayoutKind.Sequential)] struct BasicLimits {
        public long processTime, jobTime; public uint flags;
        public UIntPtr minWorking, maxWorking; public uint active;
        public UIntPtr affinity; public uint priority, scheduling;
    }
    [StructLayout(LayoutKind.Sequential)] struct Counters {
        public ulong readOperations, writeOperations, otherOperations;
        public ulong readBytes, writeBytes, otherBytes;
    }
    [StructLayout(LayoutKind.Sequential)] struct ExtendedLimits {
        public BasicLimits basic; public Counters io;
        public UIntPtr processMemory, jobMemory, peakProcessMemory, peakJobMemory;
    }
    [StructLayout(LayoutKind.Sequential)] struct Accounting {
        public long user, kernel, periodUser, periodKernel;
        public uint faults, total, active, terminated;
    }
    [StructLayout(LayoutKind.Sequential)] struct FileTime { public uint low, high; }
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool CreateProcessW(string application, StringBuilder command,
        IntPtr processAttributes, IntPtr threadAttributes, [MarshalAs(UnmanagedType.Bool)] bool inherit,
        uint flags, IntPtr environment, string directory, ref StartupEx startup, out ProcessInfo process);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, ExactSpelling=true, SetLastError=true)]
    static extern IntPtr CreateJobObjectW(IntPtr attributes, string name);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool SetInformationJobObject(IntPtr job, int informationClass,
        ref ExtendedLimits information, uint size);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool QueryInformationJobObject(IntPtr job, int informationClass,
        out Accounting information, uint size, out uint returned);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool TerminateJobObject(IntPtr job, uint status);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool TerminateProcess(IntPtr process, uint status);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    static extern uint ResumeThread(IntPtr thread);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool GetExitCodeProcess(IntPtr process, out uint code);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool GetProcessTimes(IntPtr process, out FileTime created, out FileTime exited,
        out FileTime kernel, out FileTime user);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool InitializeProcThreadAttributeList(IntPtr attributes, uint count,
        uint flags, ref UIntPtr size);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool UpdateProcThreadAttribute(IntPtr attributes, uint flags,
        UIntPtr attribute, IntPtr value, UIntPtr size, IntPtr previous, IntPtr returned);
    [DllImport("kernel32.dll", ExactSpelling=true)]
    static extern void DeleteProcThreadAttributeList(IntPtr attributes);
    [DllImport("kernel32.dll", ExactSpelling=true)] static extern IntPtr GetStdHandle(int kind);
    [DllImport("kernel32.dll", ExactSpelling=true)] static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool DuplicateHandle(IntPtr sourceProcess, IntPtr source, IntPtr destinationProcess,
        out IntPtr destination, uint access, [MarshalAs(UnmanagedType.Bool)] bool inherit, uint options);
    [DllImport("kernel32.dll", ExactSpelling=true, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)] static extern bool CloseHandle(IntPtr handle);

    // CopyToAsync never retains more than this actual byte extent. Overflow
    // faults its task and the controlling deadline kills only this owned job.
    sealed class Capture : Stream {
        readonly MemoryStream bytes = new MemoryStream();
        readonly int capacity; readonly object gate = new object();
        public Capture(int limit) { capacity = limit; }
        public byte[] Snapshot() { lock(gate) { return bytes.ToArray(); } }
        public override void Write(byte[] b, int offset, int count) {
            lock(gate) {
                if(count > capacity - bytes.Length) throw new IOException("owned output extent exceeded");
                bytes.Write(b, offset, count);
            }
        }
        public override bool CanRead { get { return false; } }
        public override bool CanSeek { get { return false; } }
        public override bool CanWrite { get { return true; } }
        public override long Length { get { lock(gate) { return bytes.Length; } } }
        public override long Position { get { return Length; } set { throw new NotSupportedException(); } }
        public override void Flush() {}
        public override int Read(byte[] b, int o, int c) { throw new NotSupportedException(); }
        public override long Seek(long o, SeekOrigin s) { throw new NotSupportedException(); }
        public override void SetLength(long n) { throw new NotSupportedException(); }
    }
    IntPtr job, process;
    AnonymousPipeServerStream input, output, error;
    Capture capturedOut, capturedError;
    Task outTask, errorTask;
    bool disposed;
    public uint Id { get; private set; }
    public ulong CreationFileTime { get; private set; }
    public bool AssignedBeforeResume { get; private set; }
    public bool ConsoleMode { get; private set; }
    static void Check(bool value) {
        if(!value) throw new Win32Exception(Marshal.GetLastWin32Error(), "owned process API failed");
    }
    static void Close(ref IntPtr value) {
        if(value != IntPtr.Zero) { IntPtr old = value; value = IntPtr.Zero; Check(CloseHandle(old)); }
    }
    public bool WaitForExit(int milliseconds) {
        if(process == IntPtr.Zero || milliseconds < 0) throw new InvalidOperationException("no owned process/deadline");
        uint code = WaitForSingleObject(process, (uint)milliseconds);
        if(code == 0) return true;
        if(code == 258) return false;
        throw new Win32Exception(Marshal.GetLastWin32Error(), "owned process wait failed");
    }
    public bool HasExited { get { return WaitForExit(0); } }
    public int ExitCode {
        get { uint status; Check(GetExitCodeProcess(process, out status)); return unchecked((int)status); }
    }
    public bool CaptureFaulted {
        get { return !ConsoleMode && (outTask.IsFaulted || errorTask.IsFaulted); }
    }
    public uint ActiveProcesses {
        get {
            Accounting accounting; uint returned;
            Check(QueryInformationJobObject(job, 1, out accounting, (uint)Marshal.SizeOf(typeof(Accounting)), out returned));
            if(returned != Marshal.SizeOf(typeof(Accounting))) throw new InvalidOperationException("job accounting extent changed");
            return accounting.active;
        }
    }
    public void Kill() { Check(TerminateJobObject(job, 124)); }
    public uint RetireJob(int milliseconds) {
        if(ActiveProcesses != 0) Kill();
        Stopwatch clock = Stopwatch.StartNew();
        while(ActiveProcesses != 0 && clock.ElapsedMilliseconds < milliseconds) Thread.Sleep(10);
        uint active = ActiveProcesses;
        if(active != 0) throw new TimeoutException("owned job retirement deadline exceeded");
        return active;
    }
    public bool Drain(int milliseconds) {
        return ConsoleMode || Task.WaitAll(new Task[] { outTask, errorTask }, milliseconds);
    }
    public void WriteInputLine(string text, int milliseconds) {
        if(ConsoleMode || input == null || text == null || milliseconds <= 0 || milliseconds > 5000)
            throw new InvalidOperationException("no owned bounded interactive input");
        byte[] bytes = Encoding.UTF8.GetBytes(text + "\n");
        if(bytes.Length > 513) throw new InvalidOperationException("interactive command extent exceeded");
        Task write = input.WriteAsync(bytes, 0, bytes.Length);
        if(!write.Wait(milliseconds)) throw new TimeoutException("interactive stdin deadline exceeded");
    }
    public void CloseInput() { if(input != null) { input.Dispose(); input = null; } }
    public void SendInput(string text, int milliseconds) {
        if(ConsoleMode || input == null) throw new InvalidOperationException("no owned anonymous stdin");
        if(text != null) {
            byte[] bytes = Encoding.UTF8.GetBytes(text + "\n");
            if(bytes.Length > 4096) throw new InvalidOperationException("private input extent exceeded");
            Task write = input.WriteAsync(bytes, 0, bytes.Length);
            if(!write.Wait(milliseconds)) throw new TimeoutException("owned stdin deadline exceeded");
        }
        input.Dispose(); input = null;
    }
    public byte[] StdoutBytes() { return capturedOut.Snapshot(); }
    public byte[] StderrBytes() { return capturedError.Snapshot(); }
    public void Dispose() {
        if(disposed) return; disposed = true;
        // No exception path can leave a runnable child: the sole job handle
        // is non-inheritable and KILL_ON_JOB_CLOSE even if accounting fails.
        try { if(job != IntPtr.Zero) { RetireJob(10000); } }
        finally {
            try { Close(ref job); }
            finally {
                try { Close(ref process); }
                finally {
                    if(input != null) input.Dispose();
                    if(output != null) output.Dispose();
                    if(error != null) error.Dispose();
                }
            }
        }
    }
    public static UwvmWindowsDebugAcceptanceOwnedProcessR1 Start(string application, string command, bool console) {
        if(IntPtr.Size != 8 || Marshal.SizeOf(typeof(Startup)) != 104 ||
            Marshal.SizeOf(typeof(StartupEx)) != 112 || Marshal.SizeOf(typeof(ProcessInfo)) != 24 ||
            Marshal.SizeOf(typeof(ExtendedLimits)) != 144 || Marshal.SizeOf(typeof(Accounting)) != 48)
            throw new InvalidOperationException("actual Win64 SDK interop layout required");
        if(!Path.IsPathRooted(application) || application.IndexOf('\0') >= 0 || command.Length >= 32767)
            throw new InvalidOperationException("exact executable and bounded native command required");
        UwvmWindowsDebugAcceptanceOwnedProcessR1 owned = new UwvmWindowsDebugAcceptanceOwnedProcessR1();
        IntPtr attributes = IntPtr.Zero, handles = IntPtr.Zero;
        IntPtr[] inherited = new IntPtr[3];
        bool initialized = false;
        ProcessInfo created = new ProcessInfo();
        try {
            owned.job = CreateJobObjectW(IntPtr.Zero, null); Check(owned.job != IntPtr.Zero);
            ExtendedLimits limits = new ExtendedLimits(); limits.basic.flags = 0x2000;
            Check(SetInformationJobObject(owned.job, 9, ref limits, 144));
            StartupEx startup = new StartupEx(); startup.startup.cb = 112; startup.startup.flags = 0x100;
            if(console) {
                for(int i=0; i<3; ++i) {
                    IntPtr handle = GetStdHandle(-10-i);
                    Check(handle != IntPtr.Zero && handle != new IntPtr(-1));
                    Check(DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), out inherited[i], 0, true, 2));
                }
            } else {
                owned.input = new AnonymousPipeServerStream(PipeDirection.Out, HandleInheritability.Inheritable);
                owned.output = new AnonymousPipeServerStream(PipeDirection.In, HandleInheritability.Inheritable);
                owned.error = new AnonymousPipeServerStream(PipeDirection.In, HandleInheritability.Inheritable);
                inherited[0] = owned.input.ClientSafePipeHandle.DangerousGetHandle();
                inherited[1] = owned.output.ClientSafePipeHandle.DangerousGetHandle();
                inherited[2] = owned.error.ClientSafePipeHandle.DangerousGetHandle();
            }
            startup.startup.input = inherited[0]; startup.startup.output = inherited[1]; startup.startup.error = inherited[2];
            UIntPtr size = UIntPtr.Zero;
            InitializeProcThreadAttributeList(IntPtr.Zero, 1, 0, ref size);
            if(size.ToUInt64() == 0 || size.ToUInt64() > 65536) throw new InvalidOperationException("bounded handle-list size required");
            attributes = Marshal.AllocHGlobal((int)size.ToUInt64());
            Check(InitializeProcThreadAttributeList(attributes, 1, 0, ref size)); initialized = true;
            handles = Marshal.AllocHGlobal(24);
            for(int i=0; i<3; ++i) Marshal.WriteIntPtr(handles, i*8, inherited[i]);
            Check(UpdateProcThreadAttribute(attributes, 0, new UIntPtr(0x20002), handles, new UIntPtr(24), IntPtr.Zero, IntPtr.Zero));
            startup.attributes = attributes;
            uint flags = 0x00080004; // EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED
            if(!console) flags |= 0x08000000; // CREATE_NO_WINDOW, redirected owned pipes
            Check(CreateProcessW(application, new StringBuilder(command), IntPtr.Zero, IntPtr.Zero,
                true, flags, IntPtr.Zero, null, ref startup, out created));
            owned.process = created.process; owned.Id = created.pid; owned.ConsoleMode = console;
            FileTime born, exited, kernel, user;
            Check(GetProcessTimes(owned.process, out born, out exited, out kernel, out user));
            owned.CreationFileTime = ((ulong)born.high << 32) | born.low;
            Check(AssignProcessToJobObject(owned.job, owned.process)); owned.AssignedBeforeResume = true;
            if(!console) {
                owned.input.DisposeLocalCopyOfClientHandle();
                owned.output.DisposeLocalCopyOfClientHandle();
                owned.error.DisposeLocalCopyOfClientHandle();
                owned.capturedOut = new Capture(4194304); owned.capturedError = new Capture(4194304);
                owned.outTask = owned.output.CopyToAsync(owned.capturedOut);
                owned.errorTask = owned.error.CopyToAsync(owned.capturedError);
            }
            if(ResumeThread(created.thread) == UInt32.MaxValue) Check(false);
        } catch {
            // Before Assign succeeds the child is still suspended and the
            // CreateProcess handle is its sole authority, never a guessed PID.
            try {
                if(owned.process != IntPtr.Zero && !owned.AssignedBeforeResume) {
                    Check(TerminateProcess(owned.process, 124));
                    if(!owned.WaitForExit(10000)) throw new TimeoutException("unadmitted owned child did not retire");
                }
            } finally { owned.Dispose(); }
            throw;
        } finally {
            bool cleanupFailed = false;
            try { Close(ref created.thread); } catch { cleanupFailed = true; }
            if(initialized) DeleteProcThreadAttributeList(attributes);
            if(attributes != IntPtr.Zero) Marshal.FreeHGlobal(attributes);
            if(handles != IntPtr.Zero) Marshal.FreeHGlobal(handles);
            if(console) for(int i=0; i<3; ++i) {
                try { Close(ref inherited[i]); } catch { cleanupFailed = true; }
            }
            if(cleanupFailed) {
                // A failed cleanup cannot discard an otherwise live owner.
                owned.Dispose();
                throw new InvalidOperationException("owned launch-handle cleanup failed");
            }
        }
        return owned;
    }
}
'@
    $script:currentDebugOwnedTypesInitialized = $true
}
function Quote-CurrentDebugNativeArgument([string]$Value) {
    if ($null -eq $Value -or $Value.IndexOf([char]0) -ge 0) { throw 'invalid native argv element' }
    $builder = New-Object Text.StringBuilder
    $null = $builder.Append([char]0x22)
    $slashes = 0
    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq [char]0x5c) { $slashes++; continue }
        if ($character -eq [char]0x22) { $null = $builder.Append([char]0x5c, [int](2*$slashes + 1)) }
        else { $null = $builder.Append([char]0x5c, [int]$slashes) }
        $null = $builder.Append($character); $slashes = 0
    }
    $null = $builder.Append([char]0x5c, [int](2*$slashes))
    $null = $builder.Append([char]0x22)
    return $builder.ToString()
}


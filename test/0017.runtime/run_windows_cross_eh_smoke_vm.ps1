# Run oracle-validated cross-function and multi-object EH in a real Win11 guest.
# The host qualification binds the exact PE, Wasm bytes, source and oracle.
param(
    [string]$BaseUrl = 'http://10.0.2.4:18027/uwvm-cross-eh-windows-x64',
    [string]$ArtifactRoot = '',
    [ValidateSet('ordinary', 'ros')][string]$Repository = 'ordinary',
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$QualificationSha256
)

$ErrorActionPreference = 'Stop'
$work = Join-Path $env:USERPROFILE ('Documents\uwvm-cross-eh-smoke-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $work | Out-Null
$script:rows = New-Object 'System.Collections.Generic.List[object]'
$result = [ordered]@{
    passed = $false
    source_id = $null
    product_sha256 = $null
    qualification_sha256 = $null
    oracle_sha256 = $null
    build_sha256 = $null
    runner_sha256 = $null
    qualification_schema = $null
    repository = $Repository
    os = [string](Get-CimInstance Win32_OperatingSystem).Caption
    os_version = [string](Get-CimInstance Win32_OperatingSystem).Version
    architecture = [string]$env:PROCESSOR_ARCHITECTURE
    cases = @()
}

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

# Test-only real Win32 owner; embedded bytes are covered by each runner SHA.
# CreateProcessW stays suspended until this unique non-inheritable job owns it.
# No process-name, CIM parent PID or unknown process-group kill is performed.
$script:ownedTypesInitialized = $false
function Initialize-OwnedProcessTypes {
    if ($script:ownedTypesInitialized) { return }
    if ('UwvmWindowsOwnedProcessR1' -as [type]) { throw 'fresh PowerShell host required for exact owned helper' }
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

public sealed class UwvmWindowsOwnedProcessR1 : IDisposable {
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
    public static UwvmWindowsOwnedProcessR1 Start(string application, string command, bool console) {
        if(IntPtr.Size != 8 || Marshal.SizeOf(typeof(Startup)) != 104 ||
            Marshal.SizeOf(typeof(StartupEx)) != 112 || Marshal.SizeOf(typeof(ProcessInfo)) != 24 ||
            Marshal.SizeOf(typeof(ExtendedLimits)) != 144 || Marshal.SizeOf(typeof(Accounting)) != 48)
            throw new InvalidOperationException("actual Win64 SDK interop layout required");
        if(!Path.IsPathRooted(application) || application.IndexOf('\0') >= 0 || command.Length >= 32767)
            throw new InvalidOperationException("exact executable and bounded native command required");
        UwvmWindowsOwnedProcessR1 owned = new UwvmWindowsOwnedProcessR1();
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
    $script:ownedTypesInitialized = $true
}
function Quote-NativeArgument([string]$Value) {
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
function Read-BoundedTail([string]$Path, [int]$Capacity) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return [byte[]]@() }
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $count = [int][Math]::Min([int64]$Capacity, $stream.Length)
        $null = $stream.Seek(-[int64]$count, [IO.SeekOrigin]::End)
        $bytes = New-Object byte[] $count; $used = 0
        while ($used -lt $count) {
            $got = $stream.Read($bytes, $used, $count-$used)
            if ($got -eq 0) { break }; $used += $got
        }
        if ($used -ne $count) { throw 'owned file changed while capturing bounded tail' }
        return ,$bytes
    } finally { $stream.Dispose() }
}
function Invoke-BoundedNative([string]$Executable, [string[]]$NativeArguments,
                              [string]$LogPath = '', [string]$PrivateInput = '',
                              [bool]$Sensitive = $false, [int]$TimeoutMs = 90000,
                              [string]$WitnessLog = '') {
    $owned = $null; $clock = $null; $started = $false; $retired = $false
    $timedOut = $false; $drainFailed = $false; $logLimit = $false
    $code = -1; $controlFailure = ''; $before = ''; $after = ''; $active = $null
    $nativePid = $null; $birth = $null; [byte[]]$outBytes = @(); [byte[]]$errBytes = @()
    try {
        if ($TimeoutMs -le 0 -or $TimeoutMs -gt 90000) { throw 'bounded native deadline required' }
        Initialize-OwnedProcessTypes
        $before = (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToLowerInvariant()
        $command = Quote-NativeArgument $Executable
        foreach ($arg in $NativeArguments) { $command += ' ' + (Quote-NativeArgument $arg) }
        $clock = [Diagnostics.Stopwatch]::StartNew()
        $owned = [UwvmWindowsOwnedProcessR1]::Start($Executable, $command, $false)
        $started = $true; $nativePid = $owned.Id; $birth = $owned.CreationFileTime
        if (!$owned.AssignedBeforeResume) { throw 'owned job admission missing' }
        $owned.SendInput($(if ($PrivateInput) { $PrivateInput } else { $null }), 5000)
        while ($true) {
            $remaining = $TimeoutMs - $clock.ElapsedMilliseconds
            if ($remaining -le 0) { $timedOut = $true; break }
            if ($owned.WaitForExit([int][Math]::Min(100, $remaining))) {
                $retired = $true
                if ($clock.ElapsedMilliseconds -gt $TimeoutMs) { $timedOut = $true }
                break
            }
            if ($owned.CaptureFaulted) { $logLimit = $true; break }
            if ($WitnessLog -and (Test-Path -LiteralPath $WitnessLog -PathType Leaf) -and
                (Get-Item -LiteralPath $WitnessLog).Length -gt 8388608) { $logLimit = $true; break }
        }
        if ($timedOut -or $logLimit) {
            $owned.Kill(); $retired = $owned.WaitForExit(10000)
        }
        if (!$retired) { throw 'owned process did not retire within deadline' }
        $code = $owned.ExitCode
        # Native root retirement alone does not prove launcher/adapter children
        # have gone. This query/termination concerns only the pre-admitted job.
        $active = $owned.RetireJob(10000)
        if (!$owned.Drain(10000)) { $drainFailed = $true }
        $outBytes = $owned.StdoutBytes(); $errBytes = $owned.StderrBytes()
    } catch {
        $controlFailure = if ($Sensitive) { 'private owned process failed' } else { [string]$_ }
    } finally {
        if ($owned) {
            try {
                $active = $owned.RetireJob(10000)
                $retired = $owned.WaitForExit(10000)
                $code = $owned.ExitCode
                if (!$owned.Drain(10000)) { $drainFailed = $true }
                $outBytes = $owned.StdoutBytes(); $errBytes = $owned.StderrBytes()
            } catch { $controlFailure += '; owned retirement/drain failed' }
            try { $owned.Dispose() } catch { $controlFailure += '; owned handles did not retire' }
        }
        if ($clock) { $clock.Stop() }
        $PrivateInput = $null
    }
    $elapsed = if ($clock) { $clock.ElapsedMilliseconds } else { 0 }
    if ($null -eq $outBytes) { $outBytes = [byte[]]@() }
    if ($null -eq $errBytes) { $errBytes = [byte[]]@() }
    $text = if ($Sensitive) { '' } else { [Text.Encoding]::UTF8.GetString($outBytes) + "`n" + [Text.Encoding]::UTF8.GetString($errBytes) }
    try {
        $after = (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToLowerInvariant()
        if (!$Sensitive -and $LogPath) {
            [IO.File]::WriteAllBytes($LogPath + '.stdout.log', $outBytes)
            [IO.File]::WriteAllBytes($LogPath + '.stderr.log', $errBytes)
            [IO.File]::WriteAllText($LogPath, $text, (New-Object Text.UTF8Encoding($false)))
        }
    } catch { $controlFailure += '; evidence capture changed' }
    # Feature-gate rejections use a normal small exit code; AV, IL and other
    # NTSTATUS exception exits cannot masquerade as a successful negative test.
    $nativeException = ($code -lt 0)
    $ok = ($started -and $retired -and $active -eq 0 -and !$timedOut -and !$logLimit -and
           !$drainFailed -and !$controlFailure -and !$nativeException -and $before -eq $after)
    $row = [ordered]@{ process_started=$started; owned_process_retired=$retired;
        owned_job_active_after_retirement=$active; assigned_before_resume=$started;
        hard_capture_limit_per_stream=4194304;
        process_id=$nativePid; creation_filetime=$birth; timed_out=$timedOut; timeout_ms=$TimeoutMs;
        elapsed_ms=$elapsed; log_limit_exceeded=$logLimit; sampled_log_limit_bytes=8388608;
        stream_drain_failed=$drainFailed; controller_error=$controlFailure; evidence_error='';
        unexpected_native_exception=$nativeException; product_sha256_before=$before;
        product_sha256_after=$after; exit_code=$code; boundary_ok=[bool]$ok;
        sensitive_output_not_persisted=$Sensitive; raw_streams=[ordered]@{} }
    if (!$Sensitive -and $LogPath) {
        foreach ($kind in @('stdout', 'stderr')) {
            $path = $LogPath + '.' + $kind + '.log'
            if (Test-Path -LiteralPath $path -PathType Leaf) {
                $row.raw_streams[$kind] = @{path=$path;bytes=(Get-Item -LiteralPath $path).Length;
                    sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
            }
        }
    }
    return [pscustomobject]@{ boundary=$row; exit_code=$code; stdout=[Text.Encoding]::UTF8.GetString($outBytes);
        stderr=$(if ($Sensitive) { '' } else { [Text.Encoding]::UTF8.GetString($errBytes) }) }
}
# End embedded windows_owned_process.ps1

function Check-Run([string]$Name, [string[]]$Arguments, [bool]$Expected,
                   [string]$Diagnostic = '', [int]$MinimumObjects = 0,
                   [bool]$RequireCacheHit = $false) {
    $log = Join-Path $work "$Name.log"
    $run = Invoke-BoundedNative $script:uwvm $Arguments $log
    $plain = [regex]::Replace($run.stdout + "`n" + $run.stderr, '\x1b\[[0-9;]*[A-Za-z]', '')
    $tail = $plain.Substring([Math]::Max(0, $plain.Length - 1200))
    $passed = ($run.boundary.boundary_ok -and (($run.exit_code -eq 0) -eq $Expected))
    if (-not $Expected -and $Diagnostic) { $passed = $passed -and $plain.ToLowerInvariant().Contains($Diagnostic) }
    $objects = 0
    foreach ($match in [regex]::Matches($plain, 'object-load-start module="[^"]*" objects=([0-9]+)')) {
        $objects = [Math]::Max($objects, [int]$match.Groups[1].Value)
    }
    $cacheHit = $plain.Contains('object-cache-hit module=')
    $nativeEH = ($plain.Contains('owning-source=yes pending-plan=native') -and
                 $plain.Contains('body-fallback=no') -and
                 $plain.Contains('finalize-object-end module='))
    if ($MinimumObjects -gt 0) { $passed = $passed -and $objects -ge $MinimumObjects -and $nativeEH }
    if ($RequireCacheHit) { $passed = $passed -and $cacheHit }
    $row = $run.boundary
    $row.name = $Name; $row.expected_success = $Expected; $row.diagnostic = $Diagnostic
    $row.argv = @($Arguments)
    $row.passed = [bool]$passed; $row.output_tail = $tail
    $row.parallel_object_count = $objects; $row.object_cache_hit = [bool]$cacheHit
    $row.native_eh_materialized = [bool]$nativeEH
    $row.log_sha256 = if (Test-Path -LiteralPath $log -PathType Leaf) {
        (Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash.ToLowerInvariant()
    } else { '' }
    $script:rows.Add($row)
    if (-not $passed) { throw "$Name failed: exit=$($run.exit_code) timeout=$($row.timed_out) retired=$($row.owned_process_retired) output=$tail" }
}


try {
    if ($ArtifactRoot) {
        Require (Test-Path -LiteralPath $ArtifactRoot -PathType Container) `
            'read-only artifact root is unavailable'
    }
    $qualificationFile = Join-Path $work 'qualification.json'
    if ($ArtifactRoot) {
        Copy-Item -LiteralPath (Join-Path $ArtifactRoot 'qualification.json') `
            -Destination $qualificationFile -Force
    } else {
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 30 "$BaseUrl/qualification.json" `
            -OutFile $qualificationFile | Out-Null
    }
    Require ((Get-FileHash $qualificationFile -Algorithm SHA256).Hash -ieq $QualificationSha256) `
        'cross-function EH qualification changed'
    $qualification = Get-Content -Raw $qualificationFile | ConvertFrom-Json
    Require ($qualification.schema -in @(1, 2) -and
             $qualification.source_id -match '^sha256:[0-9a-fA-F]{64}$' -and
             $qualification.product_sha256 -match '^[0-9a-fA-F]{64}$' -and
             $qualification.oracle_sha256 -match '^[0-9a-fA-F]{64}$' -and
             $qualification.build_sha256 -match '^[0-9a-fA-F]{64}$' -and
             $qualification.runner_sha256 -match '^[0-9a-fA-F]{64}$') `
        'cross-function EH qualification lacks exact source, product, build, runner or oracle SHA'
    $result.source_id = [string]$qualification.source_id
    $result.qualification_schema = [int]$qualification.schema
    Require ($qualification.schema -eq 2 -or $Repository -eq 'ordinary') `
        'ROS requires schema 2 with explicit repository and multi-object qualification'
    if ($qualification.schema -eq 2) {
        Require ($qualification.repository -eq $result.repository -and
                 $qualification.multi_object_required -eq $true) `
            'schema 2 requires matching repository and multi-object qualification'
    }
    $result.qualification_sha256 = (Get-FileHash $qualificationFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $result.oracle_sha256 = [string]$qualification.oracle_sha256
    $result.build_sha256 = [string]$qualification.build_sha256
    $result.runner_sha256 = (Get-FileHash $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Require ($result.runner_sha256 -ieq $qualification.runner_sha256) `
        'cross-function EH guest runner differs from qualification'
    $names = @('uwvm.exe', 'eh-cross-function-catch-ref.wasm',
               'eh-cross-function-win64-seh.wasm')
    if ($qualification.schema -eq 2) { $names += 'eh-multi-object-win64-seh.wasm' }
    foreach ($name in $names) {
        $property = $qualification.files_sha256.PSObject.Properties[$name]
        Require ($null -ne $property) "qualification lacks $name"
        $expected = [string]$property.Value
        Require ($expected -match '^[0-9a-fA-F]{64}$') "invalid $name SHA"
        $target = if ($ArtifactRoot -and $name -eq 'uwvm.exe') {
            Join-Path $ArtifactRoot $name
        } else {
            Join-Path $work $name
        }
        if ($ArtifactRoot -and $name -ne 'uwvm.exe') {
            Copy-Item -LiteralPath (Join-Path $ArtifactRoot $name) -Destination $target -Force
        } elseif (-not $ArtifactRoot) {
            Invoke-WebRequest -UseBasicParsing -TimeoutSec 30 "$BaseUrl/$name" -OutFile $target | Out-Null
        }
        Require ((Get-FileHash $target -Algorithm SHA256).Hash -ieq $expected) `
            "cross-function EH artifact changed: $name"
    }
    $script:uwvm = if ($ArtifactRoot) {
        Join-Path $ArtifactRoot 'uwvm.exe'
    } else {
        Join-Path $work 'uwvm.exe'
    }
    $result.product_sha256 = (Get-FileHash $script:uwvm -Algorithm SHA256).Hash.ToLowerInvariant()
    Require ($result.product_sha256 -ieq $qualification.product_sha256) `
        'cross-function EH executable differs from qualification'

    foreach ($policy in @('instruction', 'unwind')) {
        $compiler = if ($Repository -eq 'ros') { @('-Raot') } else { @('-Rcc', 'jit', '-Rcm', 'full') }
        $base = @('-m', 'run') + $compiler + @('-Rct', '0',
                  '-Rllvm-call-stack', $policy, '-Rllvm-cache-path', 'disable',
                  '-WFE-exceptions', '-WFE-gc')
        foreach ($stem in @('eh-cross-function-catch-ref',
                            'eh-cross-function-win64-seh')) {
            $wasm = Join-Path $work "$stem.wasm"
            $flags = if ($stem -eq 'eh-cross-function-catch-ref') {
                $base + @('-WFE-function-references')
            } else {
                $base
            }
            Check-Run "$stem-$policy" ($flags + @('--run', $wasm)) $true
            $off = @($flags | Where-Object { $_ -ne '-WFE-exceptions' }) +
                   @('-WFD-exceptions')
            Check-Run "$stem-$policy-exceptions-off" `
                ($off + @('--run', $wasm)) $false '--wasm-feature-enable-exceptions'
            if ($stem -eq 'eh-cross-function-catch-ref') {
                $off = @($flags | Where-Object { $_ -ne '-WFE-function-references' }) +
                       @('-WFD-function-references')
                Check-Run "$stem-$policy-function-references-off" `
                    ($off + @('--run', $wasm)) $false 'function-references'
            }
        }
        if ($qualification.schema -eq 2) {
            $wasm = Join-Path $work 'eh-multi-object-win64-seh.wasm'
            $parallel = @('-m', 'run') + $compiler + @('-Rct', '1',
                '-Rllvm-call-stack', $policy, '-Rclog', 'err', '-log-vb',
                '-WFE-exceptions', '-WFE-gc', '-WFE-function-references')
            Check-Run "eh-multi-object-win64-seh-$policy-uncached" `
                ($parallel + @('-Rllvm-cache-path', 'disable', '--run', $wasm)) `
                $true '' 2 $false
            $cache = Join-Path $work ("seh-object-cache-" + $policy)
            New-Item -ItemType Directory $cache | Out-Null
            foreach ($phase in @('cache-store', 'cache-load')) {
                Check-Run "eh-multi-object-win64-seh-$policy-$phase" `
                    ($parallel + @('-Rllvm-cache-path', $cache, '--run', $wasm)) `
                    $true '' 2 ($phase -eq 'cache-load')
                $stored = @(Get-ChildItem -LiteralPath $cache -Recurse -File |
                    Where-Object { $_.Length -gt 0 }).Count -gt 0
                $script:rows[$script:rows.Count - 1].cache_store_present = [bool]$stored
                Require $stored 'multi-object cache did not publish any bytes'
            }
        }
    }
    $result.passed = $true
} catch {
    $result.error = [string]$_.Exception.Message
}

$result.cases = @($script:rows.ToArray())
$json = $result | ConvertTo-Json -Depth 12 -Compress
[System.IO.File]::WriteAllText((Join-Path $work 'result.json'), $json,
    [System.Text.UTF8Encoding]::new($false))
# Preserve the local result even when the bounded host upload fails.
try {
    Invoke-WebRequest -UseBasicParsing -TimeoutSec 30 -Method Post -ContentType 'application/json' `
        -Body ([Text.Encoding]::UTF8.GetBytes($json)) "$BaseUrl/result" | Out-Null
} catch {
    $result.passed = $false
    $result.transport_error = [string]$_
    $json = $result | ConvertTo-Json -Depth 12 -Compress
    [IO.File]::WriteAllText((Join-Path $work 'result.json'), $json, (New-Object Text.UTF8Encoding($false)))
}
[Console]::WriteLine($json)
[Console]::WriteLine('Evidence directory: ' + $work)
if (-not $result.passed) { exit 1 }

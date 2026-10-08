# Execute inside the controlled Windows x64 VM. The host serves only the
# immutable PE/Wasmtime-validated Wasm inputs named below and receives one
# bounded JSON result; compilation and VM execution stay in the Linux cgroup.
param(
    [string]$BaseUrl = 'http://10.0.2.4:18018/uwvm-native-step-windows-x64',
    [string]$ArtifactRoot = '',
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$QualificationSha256
)

$ErrorActionPreference = 'Stop'
$work = Join-Path $env:USERPROFILE ('Documents\uwvm-native-step-product-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $work | Out-Null
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$result = [ordered]@{
    passed = $false
    date_utc = [DateTime]::UtcNow.ToString('o')
    os = [string](Get-CimInstance Win32_OperatingSystem).Caption
    os_version = [string](Get-CimInstance Win32_OperatingSystem).Version
    architecture = [string]$env:PROCESSOR_ARCHITECTURE
    scope = 'Real Windows x64 LLVM-full native/Wasm/source stepping, same-ABI function replacement, and protected-input alias rejection'
    runs = @()
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
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

$script:nativeBoundaries = New-Object 'System.Collections.Generic.List[object]'
function Invoke-OwnedCase([string]$Exe, [string[]]$NativeArguments, [string]$LogPath) {
    # A launcher also writes its product log through the real inherited handle;
    # keep that witness file distinct from the launcher's own bounded streams.
    $capture = if ($Exe -eq $script:uwvm) { $LogPath } else { $LogPath + '.launcher' }
    $run = Invoke-BoundedNative $Exe $NativeArguments $capture -WitnessLog $LogPath
    $run.boundary.case_log = $LogPath
    $script:nativeBoundaries.Add($run.boundary)
    if (!$run.boundary.boundary_ok) { throw 'owned native case failed its deadline/retirement/input hash' }
    if (Test-Path -LiteralPath $LogPath -PathType Leaf) {
        if ((Get-Item -LiteralPath $LogPath).Length -gt 8388608) { throw 'actual product witness log extent exceeded' }
    }
    return $run.exit_code
}

function Get-VerifiedArtifact([string]$name, [string]$path) {
    $property = $script:qualification.files_sha256.PSObject.Properties[$name]
    Require ($null -ne $property) "artifact is absent from the anchored Windows manifest: $name"
    $expected = [string]$property.Value
    Require ($expected -match '^[0-9a-f]{64}$') "invalid anchored SHA-256 for $name"
    if ($ArtifactRoot) {
        $source = Join-Path $ArtifactRoot $name
        if ($name -eq 'uwvm.exe') {
            Require ($path -eq $source) 'PE must execute from the verified read-only root'
        } else {
            Copy-Item -LiteralPath $source -Destination $path -Force
        }
    } else {
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 30 "$BaseUrl/$name" -OutFile $path
    }
    Require ((Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -eq $expected) `
        "downloaded Windows input changed before execution: $name"
}

try {
    $qualificationFile = Join-Path $work 'qualification.json'
    if ($ArtifactRoot) {
        Require (Test-Path -LiteralPath $ArtifactRoot -PathType Container) `
            'local debugger artifact root is unavailable'
        Copy-Item -LiteralPath (Join-Path $ArtifactRoot 'qualification.json') `
            -Destination $qualificationFile -Force
    } else {
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 30 "$BaseUrl/qualification.json" -OutFile $qualificationFile
    }
    Require ((Get-FileHash $qualificationFile -Algorithm SHA256).Hash -ieq $QualificationSha256) `
        'Windows debugger qualification changed before guest execution'
    $script:qualification = Get-Content -Raw $qualificationFile | ConvertFrom-Json
    Require ($script:qualification.schema -eq 1 -and
             $script:qualification.source_id -match '^sha256:[0-9a-f]{64}$') `
        'Windows debugger qualification is incomplete'
    $result.source_id = [string]$script:qualification.source_id
    $result.qualification_sha256 = (Get-FileHash $qualificationFile -Algorithm SHA256).Hash
    $uwvm = if ($ArtifactRoot) { Join-Path $ArtifactRoot 'uwvm.exe' } else { Join-Path $work 'uwvm.exe' }
    $launcher = Join-Path $work 'windows_debug_product_launcher.exe'
    $wasm = Join-Path $work 'native-step-fixture.wasm'
    Get-VerifiedArtifact 'uwvm.exe' $uwvm
    Get-VerifiedArtifact 'windows_debug_product_launcher.exe' $launcher
    Get-VerifiedArtifact 'native-step-fixture.wasm' $wasm
    $result.product_sha256 = (Get-FileHash $uwvm -Algorithm SHA256).Hash
    $result.launcher_sha256 = (Get-FileHash $launcher -Algorithm SHA256).Hash
    $result.fixture_sha256 = (Get-FileHash $wasm -Algorithm SHA256).Hash

    $unsupportedLog = Join-Path $work 'debug-jit-lazy-mode-rejection.log'
    $LASTEXITCODE = Invoke-OwnedCase $uwvm @('-m', 'debug-jit', '-Rcc', 'jit', '-Rcm', 'lazy', '-Rct', '0', '-Rllvm-cache-path', 'disable', '--run', $wasm) $unsupportedLog
    $unsupportedExit = $LASTEXITCODE
    $unsupportedText = [System.IO.File]::ReadAllText($unsupportedLog)
    Require ($unsupportedExit -ne 0 -and $unsupportedText.Contains('unsupported in the current mode: llvm-jit/lazy')) `
        "debug-jit accepted a non-full LLVM mode: $unsupportedText"
    $result.runs += [ordered]@{
        policy = 'debug-jit-lazy-mode-rejected'
        exit_code = $unsupportedExit
        log_sha256 = (Get-FileHash $unsupportedLog -Algorithm SHA256).Hash
    }

    # Run genuinely new Core 3 syntax in the Windows LLVM-full product and
    # prove that its individual feature switch still rejects the same binary.
    $typedRef = Join-Path $work 'core3-typed-ref.wasm'
    $typedLog = Join-Path $work 'core3-typed-ref-product.log'
    $typedGateLog = Join-Path $work 'core3-typed-ref-feature-off.log'
    Get-VerifiedArtifact 'core3-typed-ref.wasm' $typedRef
    $LASTEXITCODE = Invoke-OwnedCase $uwvm @('-m', 'run', '-Rcc', 'jit', '-Rcm', 'full', '-Rct', '0', '-Rllvm-call-stack', 'unwind', '-Rllvm-cache-path', 'disable', '-WFE-function-references', '--run', $typedRef) $typedLog
    $typedExit = $LASTEXITCODE
    Require ($typedExit -eq 0) "Core 3 typed reference did not execute in LLVM-full: $([System.IO.File]::ReadAllText($typedLog))"
    $LASTEXITCODE = Invoke-OwnedCase $uwvm @('-m', 'validation', '-WFD-function-references', '--run', $typedRef) $typedGateLog
    $typedGateExit = $LASTEXITCODE
    $typedGateText = [System.IO.File]::ReadAllText($typedGateLog)
    Require ($typedGateExit -ne 0 -and $typedGateText.ToLowerInvariant().Contains('function-references')) `
        "Core 3 typed reference feature gate was not enforced: $typedGateText"
    $result.runs += [ordered]@{
        policy = 'core3-typed-reference-full-and-feature-gate'
        exit_code = $typedExit
        feature_off_exit_code = $typedGateExit
        fixture_sha256 = (Get-FileHash $typedRef -Algorithm SHA256).Hash
        log_sha256 = (Get-FileHash $typedLog -Algorithm SHA256).Hash
        feature_off_log_sha256 = (Get-FileHash $typedGateLog -Algorithm SHA256).Hash
    }

    # After a branch, Core 3's polymorphic reference bottom is a legal
    # throw_ref operand. Both JIT call-stack policies must accept that bytecode.
    $throwRef = Join-Path $work 'core3-throw-ref-bottom.wasm'
    Get-VerifiedArtifact 'core3-throw-ref-bottom.wasm' $throwRef
    foreach ($policy in @('instruction', 'unwind')) {
        $throwLog = Join-Path $work "core3-throw-ref-bottom-$policy.log"
        $LASTEXITCODE = Invoke-OwnedCase $uwvm @('-m', 'run', '-Rcc', 'jit', '-Rcm', 'full', '-Rct', '0', '-Rllvm-call-stack', $policy, '-Rllvm-cache-path', 'disable', '-WFE-function-references', '-WFE-exceptions', '--run', $throwRef) $throwLog
        $throwExit = $LASTEXITCODE
        Require ($throwExit -eq 0) "Core 3 polymorphic throw_ref failed under $policy : $([System.IO.File]::ReadAllText($throwLog))"
        $result.runs += [ordered]@{
            policy = "core3-throw-ref-bottom-$policy"
            exit_code = $throwExit
            fixture_sha256 = (Get-FileHash $throwRef -Algorithm SHA256).Hash
            log_sha256 = (Get-FileHash $throwLog -Algorithm SHA256).Hash
        }
    }
    $throwGateLog = Join-Path $work 'core3-throw-ref-bottom-feature-off.log'
    $LASTEXITCODE = Invoke-OwnedCase $uwvm @('-m', 'validation', '-WFE-function-references', '-WFD-exceptions', '--run', $throwRef) $throwGateLog
    $throwGateExit = $LASTEXITCODE
    $throwGateText = [System.IO.File]::ReadAllText($throwGateLog)
    Require ($throwGateExit -ne 0 -and $throwGateText.ToLowerInvariant().Contains('exceptions')) `
        "Core 3 exception feature gate was not enforced: $throwGateText"
    $result.runs += [ordered]@{
        policy = 'core3-throw-ref-bottom-exceptions-feature-off'
        exit_code = $throwGateExit
        log_sha256 = (Get-FileHash $throwGateLog -Algorithm SHA256).Hash
    }

    # br_on_null consumes a polymorphic bottom after an unconditional branch;
    # the following throw_ref must remain type-correct in both stack policies.
    $branchNull = Join-Path $work 'core3-br-on-null-bottom.wasm'
    Get-VerifiedArtifact 'core3-br-on-null-bottom.wasm' $branchNull
    foreach ($policy in @('instruction', 'unwind')) {
        $branchLog = Join-Path $work "core3-br-on-null-bottom-$policy.log"
        $LASTEXITCODE = Invoke-OwnedCase $uwvm @('-m', 'run', '-Rcc', 'jit', '-Rcm', 'full', '-Rct', '0', '-Rllvm-call-stack', $policy, '-Rllvm-cache-path', 'disable', '-WFE-function-references', '-WFE-exceptions', '--run', $branchNull) $branchLog
        $branchExit = $LASTEXITCODE
        Require ($branchExit -eq 0) "Core 3 polymorphic br_on_null/throw_ref failed under $policy : $([System.IO.File]::ReadAllText($branchLog))"
        $result.runs += [ordered]@{
            policy = "core3-br-on-null-bottom-$policy"
            exit_code = $branchExit
            fixture_sha256 = (Get-FileHash $branchNull -Algorithm SHA256).Hash
            log_sha256 = (Get-FileHash $branchLog -Algorithm SHA256).Hash
        }
    }
    $branchGateLog = Join-Path $work 'core3-br-on-null-bottom-feature-off.log'
    $LASTEXITCODE = Invoke-OwnedCase $uwvm @('-m', 'validation', '-WFE-function-references', '-WFD-exceptions', '--run', $branchNull) $branchGateLog
    $branchGateExit = $LASTEXITCODE
    $branchGateText = [System.IO.File]::ReadAllText($branchGateLog)
    Require ($branchGateExit -ne 0 -and $branchGateText.ToLowerInvariant().Contains('exceptions')) `
        "Core 3 br_on_null exception feature gate was not enforced: $branchGateText"
    $result.runs += [ordered]@{
        policy = 'core3-br-on-null-bottom-exceptions-feature-off'
        exit_code = $branchGateExit
        log_sha256 = (Get-FileHash $branchGateLog -Algorithm SHA256).Hash
    }
    $branchRefsGateLog = Join-Path $work 'core3-br-on-null-bottom-function-references-off.log'
    $LASTEXITCODE = Invoke-OwnedCase $uwvm @('-m', 'validation', '-WFD-function-references', '-WFE-exceptions', '--run', $branchNull) $branchRefsGateLog
    $branchRefsGateExit = $LASTEXITCODE
    $branchRefsGateText = [System.IO.File]::ReadAllText($branchRefsGateLog)
    Require ($branchRefsGateExit -ne 0 -and $branchRefsGateText.ToLowerInvariant().Contains('function-references')) `
        "Core 3 br_on_null function-references feature gate was not enforced: $branchRefsGateText"
    $result.runs += [ordered]@{
        policy = 'core3-br-on-null-bottom-function-references-off'
        exit_code = $branchRefsGateExit
        log_sha256 = (Get-FileHash $branchRefsGateLog -Algorithm SHA256).Hash
    }

    foreach ($policy in @('instruction', 'unwind')) {
        $commandFile = Join-Path $work "$policy-commands.txt"
        $log = Join-Path $work "$policy-product.log"
        # The loop's expression byte 1 is its block-type immediate (0x40),
        # never an executable Wasm instruction or a debugger safe point.
        # Each wait has a two-second bound. Repeating it tolerates QEMU/JIT
        # startup while every later wait returns immediately once parked.
        $commands = @('step asm 1', 'break 0 0 1', 'break 0 0 0', 'continue') +
                    (@('wait') * 8) +
                    @('step asm 1', 'step asm 1', 'status', 'step wasm 1', 'quit')
        [System.IO.File]::WriteAllText($commandFile, (($commands -join "`n") + "`n"), [System.Text.Encoding]::ASCII)
        $LASTEXITCODE = Invoke-OwnedCase $launcher @($uwvm, $wasm, $log, $policy, $commandFile) $log
        $exitCode = $LASTEXITCODE
        $text = [System.IO.File]::ReadAllText($log)
        Require ($exitCode -eq 0) "$policy product exited $exitCode; $text"
        Require ($text.Contains('error: command or thread is not valid')) "$policy accepted a step before thread admission"
        Require ($text.Contains('error: Wasm byte offset has no emitted executable debug safe point')) `
            "$policy accepted the loop block-type immediate as an executable breakpoint: $text"
        Require ($text.Contains('stopped: breakpoint')) "$policy breakpoint was not reached"
        Require ($text.Contains('stopped: selected participant step')) "$policy Wasm step did not stop"
        $wasmStep = [regex]::Match($text, 'stopped: selected participant step[\s\S]*?thread 1 module=0 function=0 byte-offset=([0-9]+)')
        Require ($wasmStep.Success) "$policy Wasm step has no concrete function/offset: $text"
        $executableOffset = [int]$wasmStep.Groups[1].Value
        Require ($executableOffset -gt 0 -and $executableOffset -lt 6) `
            "$policy Wasm step did not reach a nonzero instruction in the loop: $text"
        $steps = [regex]::Matches($text, 'native instruction 0x([0-9a-f]+) bytes=([0-9a-f ]+)  ([^\r\n]+?) -> 0x([0-9a-f]+)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        Require ($steps.Count -eq 2) "$policy expected two decoded native instructions; got $($steps.Count): $text"
        Require ($steps[0].Groups[4].Value -eq $steps[1].Groups[1].Value) "$policy did not advance to the next native PC"
        Require (([regex]::Matches($text, 'stopped: native instruction step')).Count -eq 2) "$policy native trap count is not two"
        foreach ($step in $steps) {
            $octets = $step.Groups[2].Value.Trim().Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries)
            Require ($octets.Count -ge 1 -and $octets.Count -le 15) "$policy invalid instruction byte length"
            Require ([regex]::IsMatch($step.Groups[3].Value, '[A-Za-z]')) "$policy missing mnemonic"
        }
        $result.runs += [ordered]@{
            policy = $policy
            exit_code = $exitCode
            first_pc = $steps[0].Groups[1].Value
            first_bytes = $steps[0].Groups[2].Value
            first_asm = $steps[0].Groups[3].Value
            second_pc = $steps[1].Groups[1].Value
            second_bytes = $steps[1].Groups[2].Value
            second_asm = $steps[1].Groups[3].Value
            wasm_executable_offset = $executableOffset
            log_sha256 = (Get-FileHash $log -Algorithm SHA256).Hash
        }
        $result.runs += [ordered]@{
            policy = "$policy-non-executable-offset-rejected"
            rejected_offset = 1
            log_sha256 = (Get-FileHash $log -Algorithm SHA256).Hash
        }

        # Register the nonzero offset observed at an actual Wasm safe point
        # in a fresh process. A hit proves the address is executable and the
        # breakpoint survives JIT publication under this call-stack policy.
        $offsetCommands = Join-Path $work "$policy-nonzero-breakpoint-commands.txt"
        $offsetLog = Join-Path $work "$policy-nonzero-breakpoint-product.log"
        $offsetLines = @("break 0 0 $executableOffset", 'continue') +
                       (@('wait') * 8) + @('status', 'quit')
        [System.IO.File]::WriteAllText($offsetCommands, (($offsetLines -join "`n") + "`n"), $utf8NoBom)
        $LASTEXITCODE = Invoke-OwnedCase $launcher @($uwvm, $wasm, $offsetLog, $policy, $offsetCommands) $offsetLog
        $offsetExit = $LASTEXITCODE
        $offsetText = [System.IO.File]::ReadAllText($offsetLog)
        Require ($offsetExit -eq 0 -and $offsetText.Contains('breakpoint 1 registered at executable Wasm expression byte offset')) `
            "$policy nonzero breakpoint registration failed: $offsetText"
        Require ($offsetText.Contains('stopped: breakpoint') -and
                 $offsetText.Contains("thread 1 module=0 function=0 byte-offset=$executableOffset")) `
            "$policy nonzero breakpoint did not stop at its exact Wasm instruction: $offsetText"
        $result.runs += [ordered]@{
            policy = "$policy-nonzero-executable-breakpoint"
            executable_offset = $executableOffset
            exit_code = $offsetExit
            log_sha256 = (Get-FileHash $offsetLog -Algorithm SHA256).Hash
        }
    }

    $aliasCommands = Join-Path $work 'alias-commands.txt'
    $aliasLog = Join-Path $work 'alias-product.log'
    [System.IO.File]::WriteAllText($aliasCommands, "quit`n", [System.Text.Encoding]::ASCII)
    $LASTEXITCODE = Invoke-OwnedCase $launcher @($uwvm, $wasm, $aliasLog, 'alias', $aliasCommands) $aliasLog
    $aliasExit = $LASTEXITCODE
    $aliasText = [System.IO.File]::ReadAllText($aliasLog)
    Require ($aliasExit -ne 0) 'protected input and output alias was accepted'
    Require ($aliasText.Contains('Unable to isolate debug-jit command input')) "missing protected-input rejection: $aliasText"
    $result.runs += [ordered]@{
        policy = 'rejected-input-output-alias'
        exit_code = $aliasExit
        log_sha256 = (Get-FileHash $aliasLog -Algorithm SHA256).Hash
    }

    # Windows does not accept the Linux inherited-FD late attach protocol. A
    # guessed numeric handle must fail before guest entry rather than becoming
    # an unauthenticated debugger channel.
    $lateLog = Join-Path $work 'late-attach-refusal.log'
    $LASTEXITCODE = Invoke-OwnedCase $uwvm @('-m', 'run', '-Rct', '0', '-Rllvm-call-stack', 'unwind', '-Rllvm-cache-path', 'disable', '--debug-jit-control-fd', '3', '--run', $wasm) $lateLog
    $lateExit = $LASTEXITCODE
    $lateText = [System.IO.File]::ReadAllText($lateLog)
    Require ($lateExit -ne 0) 'Windows accepted an inherited-FD late attach request'
    Require ($lateText.Contains('debug-jit control FD is unsupported in the current mode')) `
        "Windows late attach did not fail closed: $lateText"
    $result.runs += [ordered]@{
        policy = 'late-attach-fd-rejected'
        exit_code = $lateExit
        log_sha256 = (Get-FileHash $lateLog -Algorithm SHA256).Hash
    }

    # A numeric HANDLE is never authority by itself. The VM must reject a
    # guessed value before loading guest code; only the direct parent broker's
    # inherited private message-pipe client may authorize late attachment.
    $handleLog = Join-Path $work 'late-attach-guessed-handle-refusal.log'
    $LASTEXITCODE = Invoke-OwnedCase $uwvm @('-m', 'run', '-Rct', '0', '-Rllvm-call-stack', 'unwind', '-Rllvm-cache-path', 'disable', '--debug-jit-control-handle', '3', '--run', $wasm) $handleLog
    $handleExit = $LASTEXITCODE
    $handleText = [System.IO.File]::ReadAllText($handleLog)
    Require ($handleExit -ne 0) 'Windows accepted a guessed debugger HANDLE'
    Require ($handleText.Contains('Unable to authorize debug-jit control HANDLE')) `
        "Windows guessed HANDLE was not rejected before guest entry: $handleText"
    $result.runs += [ordered]@{
        policy = 'late-attach-guessed-handle-rejected'
        exit_code = $handleExit
        log_sha256 = (Get-FileHash $handleLog -Algorithm SHA256).Hash
    }

    # The host prepares C/C++ DWARF4+5 and Rust DWARF5 Wasm in the bounded
    # cgroup. Each sidecar holds actual public function indices: wasm-ld and
    # rustc may insert helpers, so source order cannot serve as an index.
    foreach ($sourceStem in @('source-c-dwarf4', 'source-c-dwarf5',
                             'source-cpp-dwarf4', 'source-cpp-dwarf5',
                             'source-rust-dwarf5')) {
        $sourceWasm = Join-Path $work "$sourceStem.wasm"
        $sourceMetaFile = Join-Path $work "$sourceStem.meta.json"
        Get-VerifiedArtifact "$sourceStem.wasm" $sourceWasm
        Get-VerifiedArtifact "$sourceStem.meta.json" $sourceMetaFile
        $sourceMeta = Get-Content -Raw $sourceMetaFile | ConvertFrom-Json
        $entry = [int]$sourceMeta.entry
        $callee = [int]$sourceMeta.callee
        $sourceSpec = $null
        Require ($entry -ge 0 -and $callee -ge 0 -and $entry -ne $callee) `
            "$sourceStem has invalid public function indices"
        foreach ($operation in @('into', 'over', 'out')) {
            $breakIndex = if ($operation -eq 'out') { $callee } else { $entry }
            $expectedIndex = if ($operation -eq 'into') { -1 } else { $entry }
            $sourceCommands = Join-Path $work "$sourceStem-$operation-commands.txt"
            $sourceLog = Join-Path $work "$sourceStem-$operation-product.log"
            $lines = @("break 0 $breakIndex 0", 'continue') +
                     (@('wait') * 8) +
                     @("step source 1 $operation", 'status', 'quit')
            [System.IO.File]::WriteAllText($sourceCommands, (($lines -join "`n") + "`n"), $utf8NoBom)
            $LASTEXITCODE = Invoke-OwnedCase $launcher @($uwvm, $sourceWasm, $sourceLog, 'unwind', $sourceCommands) $sourceLog
            $sourceExit = $LASTEXITCODE
            $sourceText = [System.IO.File]::ReadAllText($sourceLog)
            Require ($sourceExit -eq 0) "$sourceStem $operation product exited $sourceExit; $sourceText"
            Require ($sourceText.Contains('stopped: breakpoint')) "$sourceStem $operation missed the breakpoint: $sourceText"
            $stepAt = $sourceText.LastIndexOf('stopped: selected participant step')
            Require ($stepAt -ge 0) "$sourceStem $operation did not stop after one source step: $sourceText"
            $afterStep = $sourceText.Substring($stepAt)
            if ($expectedIndex -ge 0) {
                Require ($afterStep.Contains("function=$expectedIndex")) "$sourceStem $operation stopped in the wrong function: $afterStep"
            } else {
                Require ([regex]::IsMatch($afterStep, 'function=[0-9]+')) "$sourceStem into has no guest function: $afterStep"
            }
            Require ($afterStep.Contains('  source ')) "$sourceStem $operation has no DWARF line: $afterStep"
            if ($operation -eq 'into') {
                $locations = [regex]::Matches($sourceText, '  source ([^\r\n]+)')
                Require ($locations.Count -ge 2 -and $locations[0].Groups[1].Value -ne $locations[$locations.Count - 1].Groups[1].Value) `
                    "$sourceStem into did not advance to a different source location: $sourceText"
                $firstLocation = [regex]::Match($locations[0].Groups[1].Value, '^(.+):([0-9]+):([0-9]+)$')
                Require ($firstLocation.Success -and [int]$firstLocation.Groups[2].Value -gt 0) `
                    "$sourceStem has no exact embedded DWARF file/line for a source breakpoint: $sourceText"
                $sourceSpec = "$($firstLocation.Groups[1].Value):$($firstLocation.Groups[2].Value)"
            }
            $result.runs += [ordered]@{
                policy = "$sourceStem-$operation"
                exit_code = $sourceExit
                expected_function = $expectedIndex
                log_sha256 = (Get-FileHash $sourceLog -Algorithm SHA256).Hash
            }
        }
        Require (-not [string]::IsNullOrEmpty($sourceSpec)) "$sourceStem has no source breakpoint target"
        $breakSourceCommands = Join-Path $work "$sourceStem-break-source-commands.txt"
        $breakSourceLog = Join-Path $work "$sourceStem-break-source-product.log"
        $breakSourceLines = @("break-source 0 $sourceSpec", 'continue') +
                            (@('wait') * 8) + @('status', 'quit')
        [System.IO.File]::WriteAllText($breakSourceCommands, (($breakSourceLines -join "`n") + "`n"), $utf8NoBom)
        $LASTEXITCODE = Invoke-OwnedCase $launcher @($uwvm, $sourceWasm, $breakSourceLog, 'unwind', $breakSourceCommands) $breakSourceLog
        $breakSourceExit = $LASTEXITCODE
        $breakSourceText = [System.IO.File]::ReadAllText($breakSourceLog)
        Require ($breakSourceExit -eq 0 -and $breakSourceText.Contains("breakpoint 1 source=$sourceSpec")) `
            "$sourceStem source-line breakpoint did not resolve: $breakSourceText"
        Require ($breakSourceText.Contains('stopped: breakpoint') -and
                 $breakSourceText.Contains("  source ${sourceSpec}:")) `
            "$sourceStem source-line breakpoint did not stop on the DWARF line: $breakSourceText"
        $result.runs += [ordered]@{
            policy = "$sourceStem-break-source"
            source = $sourceSpec
            exit_code = $breakSourceExit
            log_sha256 = (Get-FileHash $breakSourceLog -Algorithm SHA256).Hash
        }
    }

    # The replacement bodies are complete Wasm function bodies prepared by
    # the Linux reference runner. A malformed body and a wrong-result body
    # must leave generation 1 intact before either valid publication.
    $replacementWasm = Join-Path $work 'replace.wasm'
    Get-VerifiedArtifact 'replace.wasm' $replacementWasm
    $bodies = @{}
    foreach ($name in @('good4', 'good5', 'malformed', 'wrong_result')) {
        $body = Join-Path $work "$name.bin"
        Get-VerifiedArtifact "$name.bin" $body
        $bodies[$name] = $body
    }
    foreach ($policy in @('instruction', 'unwind')) {
        $replaceCommands = Join-Path $work "$policy-replace-commands.txt"
        $replaceLog = Join-Path $work "$policy-replace-product.log"
        $lines = @(
            "replace 0 1 1 $($bodies['malformed'])",
            "replace 0 1 1 $($bodies['wrong_result'])",
            "replace 0 1 1 $($bodies['good4'])",
            "replace 0 1 1 $($bodies['good5'])",
            "replace 0 1 2 $($bodies['good5'])",
            'continue', 'wait', 'quit')
        [System.IO.File]::WriteAllText($replaceCommands, (($lines -join "`n") + "`n"), $utf8NoBom)
        $LASTEXITCODE = Invoke-OwnedCase $launcher @($uwvm, $replacementWasm, $replaceLog, $policy, $replaceCommands) $replaceLog
        $replaceExit = $LASTEXITCODE
        $replaceText = [System.IO.File]::ReadAllText($replaceLog)
        Require ($replaceExit -eq 0) "$policy replacement product exited $replaceExit; $replaceText"
        Require (([regex]::Matches($replaceText, 'replacement body failed WebAssembly validation')).Count -eq 2) `
            "$policy did not reject both invalid bodies: $replaceText"
        Require ($replaceText.Contains('function replaced; generation 2')) "$policy first replacement failed: $replaceText"
        Require ($replaceText.Contains('function generation changed')) "$policy stale generation was accepted: $replaceText"
        Require ($replaceText.Contains('function replaced; generation 3')) "$policy second replacement failed: $replaceText"
        Require ([regex]::IsMatch($replaceText, 'replaced-value=5(?:\r?\n|$)')) `
            "$policy guest did not call the new code: $replaceText"
        $result.runs += [ordered]@{
            policy = "$policy-function-replacement"
            exit_code = $replaceExit
            log_sha256 = (Get-FileHash $replaceLog -Algorithm SHA256).Hash
        }

        # A function that is currently parked on the Wasm stack cannot be
        # replaced. Run this in a fresh process so the generation is still 1;
        # the original value printed after continue also proves no candidate
        # body was published through the rejected request.
        $activeCommands = Join-Path $work "$policy-active-frame-commands.txt"
        $activeLog = Join-Path $work "$policy-active-frame-product.log"
        $activeLines = @('break 0 1 0', 'continue') + (@('wait') * 8) +
            @("replace 0 1 1 $($bodies['good4'])", 'status', 'continue', 'wait', 'quit')
        [System.IO.File]::WriteAllText($activeCommands, (($activeLines -join "`n") + "`n"), $utf8NoBom)
        $LASTEXITCODE = Invoke-OwnedCase $launcher @($uwvm, $replacementWasm, $activeLog, $policy, $activeCommands) $activeLog
        $activeExit = $LASTEXITCODE
        $activeText = [System.IO.File]::ReadAllText($activeLog)
        Require ($activeExit -eq 0 -and $activeText.Contains('stopped: breakpoint')) `
            "$policy active-frame breakpoint was not reached: $activeText"
        Require ($activeText.Contains('active on a stopped Wasm stack')) `
            "$policy replaced a function active on a stopped Wasm stack: $activeText"
        Require ($activeText.Contains('replaced-value=3')) `
            "$policy rejected active replacement changed guest behavior: $activeText"
        $result.runs += [ordered]@{
            policy = "$policy-active-frame-replacement-rejected"
            exit_code = $activeExit
            log_sha256 = (Get-FileHash $activeLog -Algorithm SHA256).Hash
        }
    }

    # The debugger publishes a same-ABI () -> () body before guest entry;
    # two indirect generated Wasm calls then propagate its new exception to
    # try_table. Both JIT call-stack policies must catch payload 30.
    $ehReplaceWasm = Join-Path $work 'eh-hot-replace-cross-function.wasm'
    $ehReplaceBody = Join-Path $work 'eh-hot-replace-cross-function-30.bin'
    Get-VerifiedArtifact 'eh-hot-replace-cross-function.wasm' $ehReplaceWasm
    Get-VerifiedArtifact 'eh-hot-replace-cross-function-30.bin' $ehReplaceBody
    foreach ($policy in @('instruction', 'unwind')) {
        $ehBase = @('-m', 'run', '-Rcc', 'jit', '-Rcm', 'full', '-Rct', '0',
            '-Rllvm-call-stack', $policy, '-Rllvm-cache-path', 'disable')
        $baselineLog = Join-Path $work "$policy-eh-replace-baseline.log"
        $LASTEXITCODE = Invoke-OwnedCase $uwvm ($ehBase + @('-WFE-exceptions', '--run', $ehReplaceWasm)) $baselineLog
        $baselineExit = $LASTEXITCODE
        $baselineText = [System.IO.File]::ReadAllText($baselineLog)
        Require ($baselineExit -eq 0 -and
                 [regex]::IsMatch($baselineText, 'eh-replaced-value=29(?:\r?\n|$)')) `
            "$policy original cross-function EH did not catch payload 29: $baselineText"
        $disabledLog = Join-Path $work "$policy-eh-replace-exceptions-off.log"
        $LASTEXITCODE = Invoke-OwnedCase $uwvm ($ehBase + @('-WFD-exceptions', '--run', $ehReplaceWasm)) $disabledLog
        $disabledExit = $LASTEXITCODE
        $disabledText = [System.IO.File]::ReadAllText($disabledLog)
        Require ($disabledExit -ne 0 -and
                 $disabledText.ToLowerInvariant().Contains('exceptions')) `
            "$policy cross-function EH ignored the exceptions feature gate: $disabledText"
        $ehCommands = Join-Path $work "$policy-eh-replace-commands.txt"
        $ehLog = Join-Path $work "$policy-eh-replace-product.log"
        $ehLines = @('status', "replace 0 1 1 $ehReplaceBody", 'continue', 'wait', 'quit')
        [System.IO.File]::WriteAllText($ehCommands, (($ehLines -join "`n") + "`n"), $utf8NoBom)
        $LASTEXITCODE = Invoke-OwnedCase $launcher @($uwvm, $ehReplaceWasm, $ehLog, $policy, $ehCommands, 'exceptions') $ehLog
        $ehExit = $LASTEXITCODE
        $ehText = [System.IO.File]::ReadAllText($ehLog)
        $prepared = $ehText.IndexOf('prepared; no Wasm instruction executed')
        $published = $ehText.IndexOf('function replaced; generation 2')
        Require ($ehExit -eq 0 -and $prepared -ge 0 -and $published -gt $prepared -and
                 [regex]::IsMatch($ehText, 'eh-replaced-value=30(?:\r?\n|$)') -and
                 -not $ehText.Contains('active on a stopped Wasm stack') -and
                 -not $ehText.Contains('[fatal]')) `
            "$policy same-ABI cross-function EH replacement failed: $ehText"
        $result.runs += [ordered]@{
            policy = "$policy-cross-function-eh-replacement"
            baseline_exit_code = $baselineExit
            feature_off_exit_code = $disabledExit
            replacement_exit_code = $ehExit
            wasm_sha256 = (Get-FileHash $ehReplaceWasm -Algorithm SHA256).Hash
            body_sha256 = (Get-FileHash $ehReplaceBody -Algorithm SHA256).Hash
            baseline_log_sha256 = (Get-FileHash $baselineLog -Algorithm SHA256).Hash
            feature_off_log_sha256 = (Get-FileHash $disabledLog -Algorithm SHA256).Hash
            replacement_log_sha256 = (Get-FileHash $ehLog -Algorithm SHA256).Hash
        }
    }
    $result.passed = $true
} catch {
    $result.error = [string]$_.Exception.Message
}

$result.native_boundaries = @($script:nativeBoundaries.ToArray())
$json = $result | ConvertTo-Json -Depth 12 -Compress
[System.IO.File]::WriteAllText((Join-Path $work 'result.json'), $json, [System.Text.Encoding]::UTF8)
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

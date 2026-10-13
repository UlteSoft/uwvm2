from pathlib import Path
from contextlib import contextmanager
import subprocess, tarfile, shutil, hashlib, resource

def tool():
    p=Path(shutil.which('zstd'))
    assert p.is_file()
    return p

def decoder_identity():
    p=tool()
    return dict(path=str(p),sha256=hashlib.file_digest(p.open('rb'),'sha256').hexdigest(),window_limit_bytes=512<<20)

@contextmanager
def open_reader(path):
    # Bound the actual decoder window, including locally recovered archives.
    proc=subprocess.Popen([str(tool()),'-q','-d','--long=29','-c',str(path)],stdout=subprocess.PIPE)
    completed=False
    try:
        with tarfile.open(fileobj=proc.stdout,mode='r|') as t:
            yield t
        tail=0
        while data:=proc.stdout.read(65536):
            tail+=len(data)
            assert tail<=1<<20 and not any(data)
        proc.stdout.close()
        assert proc.wait(timeout=600)==0
        completed=True
    finally:
        if not completed:
            proc.terminate()
            try:proc.wait(timeout=10)
            except subprocess.TimeoutExpired:proc.kill();proc.wait()

@contextmanager
def open_writer(path):
    path=Path(path)
    assert not path.exists()
    with path.open('xb') as output:
        proc=subprocess.Popen([str(tool()),'-q','-19','--long=29','-T1','-c'],stdin=subprocess.PIPE,stdout=output)
        completed=False
        try:
            with tarfile.open(fileobj=proc.stdin,mode='w|') as t:
                yield t
            proc.stdin.close()
            assert proc.wait(timeout=600)==0
            completed=True
        finally:
            if not completed:
                proc.terminate()
                try:proc.wait(timeout=10)
                except subprocess.TimeoutExpired:proc.kill();proc.wait()
    assert path.stat().st_size<256<<20

def local_memory_upper():
    # macOS reports bytes; both streams and all their decoder children are
    # covered. Children run serially and never spawn another decoder.
    import sys
    unit=1 if sys.platform=='darwin' else 1024
    return unit*(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+
                 resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)+(128<<20)

import asyncio
from concurrent.futures import ThreadPoolExecutor
import pexpect
import queue


class NodeManager:
    def __init__(self):
        self._queue = queue.Queue()
        self._nodes = []
        self._input_thread = asyncio.get_event_loop().run_in_executor(ThreadPoolExecutor(), self._input_task)
        self._send_input_task = asyncio.create_task(self._send_input())

    async def add_node(self, node: 'Node'):
        self._nodes.append(node)
        pass

    def _input_task(self):
        while True:
            try:
                input_text = input("INPUT: ")
            except EOFError as e:
                self._queue.put("EOF")
                break
            self._queue.put(input_text)

    async def _send_input(self):
        try:
            while True:
                for node in self._nodes:
                    line = self._queue.get()+'\n'
                    await node.send_stdin(line)
        except asyncio.CancelledError:
            return

    async def stop(self):
        await self._input_thread
        self._send_input_task.cancel()
        await self._send_input_task


class Node:
    def __init__(self):
        self._ssh_proc: asyncio.Process = None
        self._is_started = False
        self._stdin: asyncio.StreamWriter = None
        self._stdout: asyncio.StreamReader = None
        self._stderr: asyncio.StreamReader = None

    async def start_node(self):
        # self._ssh_proc = await asyncio.create_subprocess_shell(' '.join(["ssh", "-l", "parkjongheum", "localhost", "cat", "/etc/os-release"]), stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
        self._ssh_proc = await asyncio.create_subprocess_shell(' '.join(["cat"]), stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
        self._stdin = self._ssh_proc.stdin
        self._stdout = self._ssh_proc.stdout
        self._stderr = self._ssh_proc.stderr
        self._is_started = True

    # async def _stdin_handler(self):
    #     while self._ssh_proc.returncode is None:
    #         try:
    #             user_input = input("STDIN : ")
    #         except EOFError:
    #             self._stdin.write_eof()
    #             break
    #         user_input += '\n'
    #         self._stdin.write(user_input.encode())
    #         await self._stdin.drain()
    #         await asyncio.sleep(0.5)

    async def send_stdin(self, text: str):
        if self._ssh_proc.returncode is None:
            if text.startswith("EOF"):
                self._stdin.write_eof()
                return
            self._stdin.write(text.encode())
            await self._stdin.drain()

    async def _stdout_handler(self):
        while True:
            line = await self._stdout.readline()
            if not line:
                break
            print("STDOUT :", line.decode().rstrip())

    async def _stderr_handler(self):
        while True:
            line = await self._stderr.readline()
            if not line:
                break
            print("STDERR :", line.decode().rstrip())
        pass

    async def node_watch(self):
        # stdin_handler = asyncio.create_task(self._stdin_handler())
        stdout_handler = asyncio.create_task(self._stdout_handler())
        stderr_handler = asyncio.create_task(self._stderr_handler())
        # await stdin_handler
        await stdout_handler
        await stderr_handler

    async def stop_node(self):
        print(await self._ssh_proc.wait())

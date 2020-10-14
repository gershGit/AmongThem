import asyncio 

class AsyncTimer:
    def __init__(self, timeout, callback):
        print("Async task starting in " + str(timeout))
        self._timeout = timeout
        self._callback = callback
        self._task = asyncio.ensure_future(self._job())

    async def _job(self):
        await asyncio.sleep(self._timeout)
        await self._callback()

    def cancel(self):
        print("Async task cancelled prematurely")
        self._task.cancel()
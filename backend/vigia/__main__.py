import asyncio
import sys

import uvicorn

from vigia.config import Settings


def main() -> None:
    if sys.platform == "win32":
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
    settings = Settings()
    uvicorn.run("vigia.main:app", host="0.0.0.0", port=settings.port)


if __name__ == "__main__":
    main()

import utils
import concurrent.futures


def get_pool(max_workers: int = 1) -> concurrent.futures.Executor:
    """get an executor pool"""

    def pool_init(v: bool):
        utils.verbose = v

    return concurrent.futures.ProcessPoolExecutor(
        max_workers=max_workers, initializer=pool_init, initargs=(utils.verbose,)
    )


class OptErr(Exception):pass

class Opt:

    def __getattr__(self, str):
        return None

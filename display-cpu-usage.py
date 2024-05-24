import math, psutil


BASE_WIDTH: int = 9
INNER_WIDTH: int = BASE_WIDTH + 14
INTERVAL: float = 0.5
HISTORY_LENGTH: int = 30

def count_digits(num: int) -> int:
	assert(num > 0)
	return int(math.log10(num)) + 1

def print_header(ncpus: int) -> None:
	header = f' CPU usage ({ncpus}):'
	print(header)
	print('┌' + '─' * (INNER_WIDTH) + '┐', end=None)
	print(f'\033[{ncpus + 1}E', end=None)
	print('└' + '─' * (INNER_WIDTH) + '┘', end=None)
	print(f'\033[{ncpus + 4}F', end=None)

def percent_to_bar(percent: float) -> str:
	# ▁ ▂ ▃ ▄ ▅ ▆ ▇ █
	if percent < 11: return '\033[100m \033[m'
	if percent < 22: return '\033[100m▁\033[m'
	if percent < 33: return '\033[100m▂\033[m'
	if percent < 44: return '\033[100m▃\033[m'
	if percent < 55: return '\033[100m▄\033[m'
	if percent < 66: return '\033[100m▅\033[m'
	if percent < 77: return '\033[100m▆\033[m'
	if percent < 88: return '\033[100m▇\033[m'
	return '█'

try:
	print('\033[?1049h\033[?25l\033[1;1H', end=None)
	
	ncpus = psutil.cpu_count()
	print_header(ncpus)
	
	average_history: list[float] = [0] * int(HISTORY_LENGTH / INTERVAL)
	percentages: list[float] = [0] * ncpus
	while True:
		total: float = 0
		for n, p in enumerate(percentages, 1):
			total += p
			align = BASE_WIDTH + 2 - count_digits(n)
			print(f'│ Core {n}: {p:>{align}.1f}%   │')
		
		print('│' + ' ' * (INNER_WIDTH) + '│')
		
		average: float = total / ncpus
		print(f'│ Average: {average:>{BASE_WIDTH}.1f}% {percent_to_bar(average)} │')
		print(f'\033[1E', end=None)
		
		print('>' + '-' * (2*ncpus + 1) + '<')
		print('┆ ' + ' '.join(map(percent_to_bar, percentages)) + ' ┆')
		print('╰' + '┄' * (2*ncpus + 1) + '╯')
		print(''.join(map(percent_to_bar, average_history)))
		print(f'\033[{ncpus + 9}F', end=None)
		
		del average_history[0]
		average_history.append(average)
		percentages = psutil.cpu_percent(interval=INTERVAL, percpu=True)
except KeyboardInterrupt:
	pass
finally:
	print('\033[?1049l\033[?25h', end=None)

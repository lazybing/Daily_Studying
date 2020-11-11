import requests
from requests.exceptions import RequestException


def get_page(url):
    #user_agent = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.193 Safari/537.36'
    #header = {}
    #header['user_agent'] = user_agent
    headers = {'User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.193 Safari/537.36'}
    try:
        response = requests.get(url, headers = headers)
        print(response)
        if response.status_code == 200:
            return response.text
        return None
    except RequestException:
        return None

if __name__ == '__main__':
    url = "https://movie.douban.com/top250"
    html = get_page(url)
    print(html)

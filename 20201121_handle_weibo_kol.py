#!/usr/bin/python
#-*- coding:UTF-8 -*-

import sys, getopt, xlrd, jieba, wordcloud, re,collections

def world_frequence(inputfile):
    txt = open(inputfile, "r", encoding='utf-8').read()
    words = jieba.lcut(txt)

    counts = {}

    for word in words:
        if len(word) == 1:
            continue
        else:
            counts[word] = counts.get(word, 0) + 1

    items = list(counts.items())
    items.sort(key=lambda x:x[1], reverse = True)
    for i in range(100):
        word, count = items[i]
        print("{0:<10}{1:>5}".format(word, count))

def openfile(filename):
    result = []
    with open(filename, 'r') as f:
        for line in f:
            result.append(line)

        for idx in range(len(result)):
            print(result[idx])

def generateciyun(inputfile):
    # 构建词云对象w，设置词云图片宽、高、字体、背景颜色等参数
    w = wordcloud.WordCloud(width=1000,
                        height=700,
                        background_color='white',
                        font_path='./simfang.ttf',
                        stopwords={'我答','我说','他说','这就像','组图共2张','组图共3张',
                                   '组图共4张','组图共5张','组图共6张','组图共7张','组图共8张',
                                   '组图共9张','原图','无锡','显示地图','风中的厂长',
                                   '落花中的雅歌','靠副业暴富的七七','红茶家的三叔','朱虹小四',
                                   '写书哥','菊厂刘掌柜','明子阿姨','索阅','Aya坨坨','维千金',
                                   '掌柜爱丽丝','打工归来啊','风远路清','上海土著一只猪',
                                   '少年伯爵','蓝烟海不仅仅会卖衣服','李美及','自我的SZ',
                                   '平常心淡定人','东北塘街区','剪枝者','野心范','迎十里',
                                   '乡下老白菜','欧阳风手记','超美超瘦的李老板','超美超瘦李老板',
                                   '村口的黑脸','堰桥街区','小姨虫虫','睡不够的黎老板','转发',
                                   '关外野马','_村西边老王_','内环幕僚长','我发表了头条文章',
                                   '观看置顶微博','打工的王者','to','and','http','cn','of','is',
                                   'the','分享图片','野马小挪','渔村希拉姐','喵小咪呀呀呀呀',
                                   '全文','腾冲','当然','也就是说','所以','A6yttCYk',
                                   '时尚搭配师Sissi','欢迎转发点赞留言','微博写作训练营','哈哈',
                                   '比如','weibo','wvr mod','weibotime type','weibo com',
                                   'mod_weibotime','xashuang2','type comment','weibotime','type',
                                   'wvr','mod','page_1005052927915845_profile','com','PS',
                                   'comment','from','随手发反馈','收藏','早安','超话'})

    #从外部.txt文件中读取大段文本，存入变量txt中
    ciyun_file = open(inputfile, encoding='utf-8')
    ciyun_txt  = ciyun_file.read()
    txtlist    = jieba.lcut(ciyun_txt)
    string     = "".join(txtlist)

    # 将txt变量传入w的generate()方法，给词云输入文字
    w.generate(ciyun_txt)

    # 将词云图片导出到当前文件夹
    ciyun_png = inputfile.replace('.txt', '.png')
    w.to_file(ciyun_png)

def handleexcel(filename):
    content_txt = filename.replace('.xlsx', '_content.txt')
    content_file = open(content_txt, "wb+")

    url_txt  = filename.replace('.xlsx', '_url.txt')
    url_file = open(url_txt, "wb+")

    with xlrd.open_workbook(filename) as data:
        table = data.sheet_by_index(0)
        rows_count = table.nrows

        for row_idx in range(1, rows_count):
            content = table.cell(row_idx, 1).value
            content_file.write(content.encode('UTF-8'))
            content_file.write("\n".encode('UTF-8'))
            content_file.write("\n".encode('UTF-8'))

            url_id = table.cell(row_idx, 0).value
            url_file.write(url_id.encode('UTF-8'))
            url_file.write("\n".encode('UTF-8'))

    generateciyun(content_txt)
    world_frequence(content_txt)

def main(argv):
    inputfile = ''
    try:
        opts,args = getopt.getopt(argv, "hi:",["ifile="])
    except getopt.GetoptError:
        print ('handle_weibo_kol.py -i <inputfile>')
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            print('handle_weibo_kol -i <inputfile>')
            sys.exit()
        elif opt in ("-i", "--ifile"):
            inputfile = arg

    handleexcel(inputfile)

if __name__ == "__main__":
    main(sys.argv[1:])

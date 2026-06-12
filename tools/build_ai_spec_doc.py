from pathlib import Path

from reportlab.graphics.shapes import Drawing, Line, Polygon, Rect, String
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import cm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    Flowable,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "generated" / "assignments"
OUT_DIR.mkdir(parents=True, exist_ok=True)

PDF_PATH = OUT_DIR / "LE3B_07_コイズミ_リョウ_敵撃破演出仕様書.pdf"
FONT_PATH = Path(r"C:\Windows\Fonts\NotoSansJP-VF.ttf")
FONT_NAME = "NotoSansJP"


class DrawingBox(Flowable):
    def __init__(self, drawing, width, height):
        super().__init__()
        self.drawing = drawing
        self.width = width
        self.height = height

    def wrap(self, avail_width, avail_height):
        return self.width, self.height

    def draw(self):
        self.drawing.drawOn(self.canv, 0, 0)


def p(text, style):
    return Paragraph(text.replace("\n", "<br/>"), style)


def explosion_diagram():
    w, h = 17.2 * cm, 5.6 * cm
    d = Drawing(w, h)
    d.add(Rect(0, 0, w, h, fillColor=colors.HexColor("#101010"), strokeColor=colors.HexColor("#2A2A2A")))
    d.add(String(18, h - 24, "敵撃破演出の流れ", fontName=FONT_NAME, fontSize=12, fillColor=colors.white))

    xs = [3.0 * cm, 8.6 * cm, 14.0 * cm]
    labels = ["命中直後", "爆発中", "演出終了"]
    sub = ["当たり判定OFF", "破片・閃光・スコア", "敵をプールへ戻す"]
    for i, x in enumerate(xs):
        if i == 0:
            d.add(Polygon([x - 25, 86, x + 20, 105, x + 40, 78, x - 5, 62], strokeColor=colors.white, fillColor=None))
            d.add(Line(x - 22, 86, x - 5, 62, strokeColor=colors.white))
            d.add(Line(x + 20, 105, x + 40, 78, strokeColor=colors.white))
        elif i == 1:
            for lx1, ly1, lx2, ly2 in [
                (-34, 0, -62, 14), (-20, 18, -38, 48), (0, 24, 0, 60),
                (20, 18, 44, 46), (34, 0, 65, 12), (-18, -16, -42, -38),
                (18, -16, 42, -36),
            ]:
                d.add(Line(x + lx1, 83 + ly1, x + lx2, 83 + ly2, strokeColor=colors.white, strokeWidth=1.1))
            for poly in [
                [x - 75, 124, x - 48, 135, x - 42, 107],
                [x + 58, 128, x + 82, 143, x + 84, 112],
                [x - 82, 58, x - 54, 47, x - 62, 77],
                [x + 50, 50, x + 84, 55, x + 62, 79],
            ]:
                d.add(Polygon(poly, strokeColor=colors.white, fillColor=None))
        else:
            d.add(String(x - 38, 85, "+100", fontName=FONT_NAME, fontSize=22, fillColor=colors.white))
            d.add(Line(x - 50, 70, x - 82, 52, strokeColor=colors.white))
            d.add(Line(x + 42, 70, x + 75, 52, strokeColor=colors.white))
        d.add(String(x - 42, 25, labels[i], fontName=FONT_NAME, fontSize=9, fillColor=colors.white))
        d.add(String(x - 55, 10, sub[i], fontName=FONT_NAME, fontSize=7.5, fillColor=colors.HexColor("#CCCCCC")))
    for x in [5.7 * cm, 11.2 * cm]:
        d.add(Line(x, 86, x + 1.2 * cm, 86, strokeColor=colors.white, strokeWidth=1.4))
        d.add(Polygon([x + 1.2 * cm, 86, x + 1.0 * cm, 94, x + 1.0 * cm, 78], strokeColor=colors.white, fillColor=colors.white))
    return DrawingBox(d, w, h)


def state_diagram():
    w, h = 17.2 * cm, 4.0 * cm
    d = Drawing(w, h)
    d.add(Rect(0, 0, w, h, fillColor=colors.HexColor("#F7F9FC"), strokeColor=colors.HexColor("#CBD5E1")))
    states = ["Alive", "HitStop", "DestroyStart", "EffectPlaying", "Removed"]
    labels = ["通常", "命中停止", "撃破開始", "演出中", "削除/再利用"]
    box_w = 2.45 * cm
    y = 55
    for i, state in enumerate(states):
        x = 0.65 * cm + i * 3.25 * cm
        d.add(Rect(x, y, box_w, 38, fillColor=colors.white, strokeColor=colors.HexColor("#1F4E79"), strokeWidth=1.0))
        d.add(String(x + 10, y + 22, state, fontName=FONT_NAME, fontSize=8.5, fillColor=colors.HexColor("#1F4E79")))
        d.add(String(x + 10, y + 9, labels[i], fontName=FONT_NAME, fontSize=7.2, fillColor=colors.HexColor("#333333")))
        if i < len(states) - 1:
            ax = x + box_w + 5
            d.add(Line(ax, y + 19, ax + 0.55 * cm, y + 19, strokeColor=colors.HexColor("#333333")))
            d.add(Polygon([ax + 0.55 * cm, y + 19, ax + 0.42 * cm, y + 24, ax + 0.42 * cm, y + 14], fillColor=colors.HexColor("#333333")))
    d.add(String(18, 25, "撃破演出中は移動・攻撃・当たり判定更新を止め、描画とエフェクトだけを更新する。", fontName=FONT_NAME, fontSize=8.2, fillColor=colors.HexColor("#333333")))
    return DrawingBox(d, w, h)


def build_pdf():
    pdfmetrics.registerFont(TTFont(FONT_NAME, str(FONT_PATH)))

    doc = SimpleDocTemplate(
        str(PDF_PATH),
        pagesize=A4,
        rightMargin=1.8 * cm,
        leftMargin=1.8 * cm,
        topMargin=1.5 * cm,
        bottomMargin=1.5 * cm,
        title="敵撃破演出仕様書",
        author="LE3B_07_コイズミ_リョウ",
    )
    styles = getSampleStyleSheet()
    title = ParagraphStyle("TitleJP", parent=styles["Title"], fontName=FONT_NAME, fontSize=20, leading=26, alignment=TA_CENTER, spaceAfter=6)
    meta = ParagraphStyle("MetaJP", parent=styles["Normal"], fontName=FONT_NAME, fontSize=9.5, leading=13, alignment=TA_CENTER, textColor=colors.HexColor("#555555"), spaceAfter=12)
    h1 = ParagraphStyle("H1JP", parent=styles["Heading1"], fontName=FONT_NAME, fontSize=13.5, leading=18, textColor=colors.HexColor("#1F4E79"), spaceBefore=9, spaceAfter=5)
    h2 = ParagraphStyle("H2JP", parent=styles["Heading2"], fontName=FONT_NAME, fontSize=11.5, leading=15, textColor=colors.HexColor("#1F4E79"), spaceBefore=7, spaceAfter=3)
    body = ParagraphStyle("BodyJP", parent=styles["BodyText"], fontName=FONT_NAME, fontSize=9.4, leading=14, alignment=TA_LEFT, spaceAfter=5, wordWrap="CJK")
    small = ParagraphStyle("SmallJP", parent=body, fontSize=7.9, leading=10.5, spaceAfter=0)
    small_center = ParagraphStyle("SmallCenterJP", parent=small, alignment=TA_CENTER)
    th = ParagraphStyle("THJP", parent=small_center, textColor=colors.white, fontSize=8.0, leading=10)

    prompt = (
        "以下で説明するゲームの「敵撃破演出」の仕様を詳しく策定してください。"
        "サンプルコードは書かず、プログラマーが実装しやすいように言葉で説明してください。"
        "現在、3Dレールシューティングゲームを制作しています。参考にしているのは「新・光神話 パルテナの鏡」の空中戦パートです。"
        "プレイヤーはマウス照準で敵を撃ち、ウェーブ制で敵が出現します。"
        "既にHPバー、弾、チャージショット、ポストエフェクト、ヒット演出、簡単な敵AIがあります。"
        "敵を倒したときの手応えを強くするため、ワイヤーフレーム風の破壊、破片、閃光、スコア表示、状態遷移、負荷対策を含む仕様書にしてください。"
    )

    story = [
        Paragraph("敵撃破演出 仕様書", title),
        Paragraph("LE3B_07_コイズミ_リョウ", meta),
        Paragraph("1. 課題内容", h1),
        p("個人制作ゲームに追加する要素について、ChatGPTを用いて仕様書を作成した。今回は、レールシューティングの爽快感を上げるための「敵撃破演出」を対象とし、実装時に迷わないように演出内容、状態遷移、発生条件、負荷対策を具体化する。", body),
        Paragraph("2. AIに渡したプロンプト", h1),
    ]

    prompt_table = Table([[p(prompt, body)]], colWidths=[17.2 * cm])
    prompt_table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#F4F8FF")),
        ("BOX", (0, 0), (-1, -1), 0.6, colors.HexColor("#D9E2F3")),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("TOPPADDING", (0, 0), (-1, -1), 7),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
    ]))
    story += [prompt_table, Spacer(1, 6), Paragraph("3. ChatGPTによる仕様書", h1)]

    story += [
        Paragraph("3.1 目的と方向性", h2),
        p("敵を倒した瞬間に、命中した手応えと撃破した気持ちよさを強化する。画面全体を派手にしすぎず、現在のワイヤーフレーム風の世界観に合うように、白い線、短い閃光、破片、スコア表示を中心に構成する。演出時間は短くし、レールシューティングのテンポを止めない。", body),
        explosion_diagram(),
        Spacer(1, 6),
        Paragraph("3.2 発生条件", h2),
    ]

    condition_rows = [
        ["条件", "内容"],
        ["発生タイミング", "敵のHPが0以下になった瞬間。複数弾が同時命中しても、撃破演出は1回だけ発生する。"],
        ["対象", "通常敵、編隊敵、中型敵に適用する。ボスは別仕様として扱う。"],
        ["無効条件", "既にDestroyStart以降の状態に入っている敵には、追加で撃破演出を発生させない。"],
        ["ゲームへの影響", "撃破演出中の敵は攻撃、移動、当たり判定を停止し、プレイヤーにダメージを与えない。"],
    ]
    story.append(make_table(condition_rows, [3.2, 14.0], small, th))

    story += [Paragraph("3.3 敵オブジェクトの状態遷移", h2), state_diagram()]
    state_rows = [
        ["状態", "仕様"],
        ["Alive", "通常状態。移動、攻撃、当たり判定、描画を行う。"],
        ["HitStop", "命中直後に2〜4フレームだけ停止する。強い弾やチャージショット命中時のみ長めにする。"],
        ["DestroyStart", "HPを0に固定し、当たり判定と攻撃処理を停止する。撃破エフェクト、スコア表示、効果音を生成する。"],
        ["EffectPlaying", "敵モデルをフェードアウトまたは非表示にし、破片と閃光のみ更新する。"],
        ["Removed", "演出終了後、敵をシーンから削除するか、オブジェクトプールへ戻して再利用する。"],
    ]
    story.append(make_table(state_rows, [3.2, 14.0], small, th))

    story += [Paragraph("3.4 演出タイムライン", h2)]
    timeline_rows = [
        ["フレーム", "処理内容"],
        ["0F", "HPが0になり、撃破フラグを立てる。当たり判定、移動、攻撃を即座に停止する。"],
        ["1〜4F", "ヒットストップ。敵とカメラをわずかに止め、命中の重さを出す。"],
        ["1〜6F", "白または薄黄色の短いフラッシュを表示する。強度は最初が最大で徐々に下げる。"],
        ["1〜18F", "ワイヤーフレームの線が外側へ広がるように分解する。破片は敵の中心からランダム方向へ飛ばす。"],
        ["6〜24F", "破片の透明度を下げながら消す。大きい敵は破片数を増やすが、上限を設ける。"],
        ["10〜40F", "敵の位置付近に「+100」などのスコア表示を出し、上方向へ移動しながらフェードアウトする。"],
        ["30〜45F", "すべての演出を終了し、敵オブジェクトを削除またはプールに戻す。"],
    ]
    story.append(make_table(timeline_rows, [2.4, 14.8], small, th))

    story += [Paragraph("3.5 構成要素", h2)]
    element_rows = [
        ["要素", "仕様"],
        ["ワイヤーフレーム分解", "敵モデルの輪郭線が一瞬だけ外側へ広がり、線がバラけるように見せる。実装が難しい場合は、あらかじめ用意した線分パーティクルで代用する。"],
        ["破片エフェクト", "三角形や四角形の小さな破片を8〜16個生成する。速度、回転、寿命をランダム化し、毎回少し違う見た目にする。"],
        ["閃光", "撃破地点に1フレームから数フレームの白いフラッシュを出す。画面全体ではなく敵周辺に限定する。"],
        ["効果音", "短く高めの電子音を鳴らす。BGMや自機の攻撃音を邪魔しない音量にする。"],
        ["スコアUI", "敵撃破位置の近くにスコアを表示する。1秒以内に消し、ワイヤーフレーム風の白い文字で統一する。"],
    ]
    story.append(make_table(element_rows, [3.4, 13.8], small, th))

    story.append(PageBreak())
    story += [Paragraph("3.6 パラメータ案", h2)]
    param_rows = [
        ["項目", "通常敵", "中型敵", "備考"],
        ["破片数", "8個", "16個", "同時撃破時は上限を設ける"],
        ["演出時間", "30F", "45F", "ゲームテンポを崩さない範囲"],
        ["ヒットストップ", "2F", "4F", "チャージ弾命中時は+2F"],
        ["フラッシュ強度", "0.6", "0.8", "画面全体ではなく敵周辺のみ"],
        ["スコア表示時間", "40F", "50F", "上へ移動しながらフェード"],
    ]
    story.append(make_table(param_rows, [3.7, 3.2, 3.2, 7.1], small, th))

    story += [
        Paragraph("3.7 実装・負荷対策", h2),
        bullet("破片やフラッシュは毎回生成せず、オブジェクトプールで管理する。", body),
        bullet("同時に大量の敵が倒れた場合、破片数とフラッシュ強度を自動で下げる。", body),
        bullet("敵撃破中の本体モデルは非表示にし、描画リストから外して負荷を下げる。", body),
        bullet("敵の種類ごとの演出差は、色、破片数、スコア値などのパラメータ変更で表現する。", body),
        bullet("最初は通常敵だけに実装し、問題がなければ中型敵やボス用に拡張する。", body),
        Paragraph("3.8 完了条件", h2),
        bullet("敵を倒した瞬間に、当たり判定と攻撃処理が停止する。", body),
        bullet("撃破演出が30〜45フレーム以内に終了し、敵がシーンから消える。", body),
        bullet("破片、閃光、効果音、スコア表示が同じタイミングで自然に発生する。", body),
        bullet("同時撃破が起きても処理落ちしにくく、ゲームの視認性を大きく損なわない。", body),
        bullet("通常敵を複数倒した場合でも、演出が重なりすぎずプレイヤーの照準操作を妨げない。", body),
        Paragraph("4. まとめ", h1),
        p("今回の仕様では、敵撃破時の見た目、音、UI、オブジェクト状態をまとめて定義した。単に敵を消すのではなく、命中停止、破片、閃光、スコア表示を短時間で組み合わせることで、レールシューティングらしい爽快感を高めることを狙う。実装時は、まず通常敵の撃破演出から作成し、その後に敵の種類ごとの違いを追加していく。", body),
    ]

    def page_num(canvas, doc_obj):
        canvas.saveState()
        canvas.setFont(FONT_NAME, 8)
        canvas.setFillColor(colors.HexColor("#777777"))
        canvas.drawRightString(19.2 * cm, 0.85 * cm, str(doc_obj.page))
        canvas.restoreState()

    doc.build(story, onFirstPage=page_num, onLaterPages=page_num)


def make_table(rows, widths_cm, small_style, header_style):
    data = []
    for row_index, row in enumerate(rows):
        style = header_style if row_index == 0 else small_style
        data.append([p(str(cell), style) for cell in row])
    table = Table(data, colWidths=[w * cm for w in widths_cm], repeatRows=1)
    commands = [
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#1F4E79")),
        ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
        ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#CBD5E1")),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("LEFTPADDING", (0, 0), (-1, -1), 5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
    ]
    for i in range(1, len(rows)):
        if i % 2 == 0:
            commands.append(("BACKGROUND", (0, i), (-1, i), colors.HexColor("#F7F9FC")))
    table.setStyle(TableStyle(commands))
    return table


def bullet(text, style):
    return Table(
        [[p("・", style), p(text, style)]],
        colWidths=[0.45 * cm, 16.75 * cm],
        style=TableStyle([
            ("VALIGN", (0, 0), (-1, -1), "TOP"),
            ("LEFTPADDING", (0, 0), (-1, -1), 0),
            ("RIGHTPADDING", (0, 0), (-1, -1), 0),
            ("TOPPADDING", (0, 0), (-1, -1), 0),
            ("BOTTOMPADDING", (0, 0), (-1, -1), 0),
        ]),
    )


if __name__ == "__main__":
    build_pdf()
    print(PDF_PATH)

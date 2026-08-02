#ifndef IOEXP_H_
#define IOEXP_H_

#ifdef __cplusplus
extern "C" {
#endif

// ===== 検証用。確認が終わったら両方0に戻すこと =====
// 起動直後に一度だけ IOCON.BANK を立て、
// レジスタマップがずれた状態を意図的に作り出す
// IOCON.BANK ずれの検証は実施済み。結果は「症状を再現できない」。
// ドライバ自身が列7の選択(0x7f)でbit7を倒すため、BANK=1は1スキャンで解消される
#define IOEXP_DEBUG_FORCE_BANK1 0
// 起動から何ms後に実行するか。
// シリアルターミナルを繋ぐ時間を確保するために遅らせる
#define IOEXP_DEBUG_FORCE_BANK1_DELAY_MS 15000
// IOCONのBANKクリアを行わない。
// 1にすると、ずれた状態が自動復帰せずそのまま維持されるので、
// 対策を入れる前の生の症状を確認できる
#define IOEXP_DEBUG_NO_IOCON_INIT 0
// I2Cの転送失敗をシリアルへ報告する (1秒に1回まで)。
// 原因調査は完了したので通常は0。再発時の観測用に残してある
#define IOEXP_DEBUG_LOG_I2C_FAIL 0

// 異常検出時にMCP23017のレジスタ内容をシリアルへ吐く。
// 出力先はNERD IDEの転送に使うUSB CDCと同じなので、
// 転送中に割り込むとIDE側の受信を乱す恐れがある。通常は0。
// 原因調査の際に1にすること
#define IOEXP_DEBUG_DUMP 0

#define IOEXP_CHRBUF 32

// キースキャンの間隔[ms]。デバウンスは2回連続一致なので、
// キー入力が確定するまでの遅延は最大 IOEXP_SCAN_INTERVAL_MS * 2
#define IOEXP_SCAN_INTERVAL_MS 10

// I2Cのタイムアウト[us]。I2C0は1MHzなので3バイトでも30us程度だが、
// 短すぎると転送の途中で打ち切ってしまい、I2Cの状態が中途半端なまま残る。
// 割り込みコンテキストでの実行をやめたので、長くても固まらない
// I2Cのタイムアウト[us]。
//
// 短くしてはいけない。転送の途中で打ち切ると、TX FIFOに積んだバイトが
// 後から流れ出てバイト列が1つずれ、MCP23017が
// 「レジスタアドレス+データ」の組を取り違えて別のレジスタを書き換える。
// 実際に2000にしたところ IODIRA が 0xff から 0x00 に化け、
// 行が出力になって全キーが押されっぱなしに見える不具合が再現した。
//
// USBの割り込みハンドラはcore0にあり、転送中はI2Cの待ちループが
// 頻繁に中断されるため、余裕を持たせる必要がある。
// 割り込みコンテキストでの実行はやめたので、長くても固まらない
#define IOEXP_I2C_TIMEOUT_US 30000

// 列の出力を切り替えてから行を読むまでの待ち時間[us]。
// 行はMCP23017の内蔵プルアップ(約100kΩ)でしか吊られておらず、
// キーマトリクスの寄生容量と合わせると立ち上がりに数十us要する。
// 待たずに読むと、実際には押されていない行がLowに見えてゴーストキーになる。
// ゴーストが出るようなら増やすこと
#define IOEXP_SETTLE_US 50

// 設定の書き戻しを試みる間隔[ms]。
// バスが固着していると1トランザクションでIOEXP_I2C_TIMEOUT_US待たされるため、
// 毎スキャン試すとCPUを食い潰して端末全体が止まって見える
#define IOEXP_RECOVER_INTERVAL_MS 500

// バスが応答しない状態が何回続いたら、
// SCLを叩く/ハードウェアリセットまで踏み込むか
// (IOEXP_RECOVER_INTERVAL_MS間隔なので4回 = 約2秒)
#define IOEXP_BUS_RECOVER_AFTER 4

// I2Cが何回連続で失敗したら復旧(ハードウェアリセット)まで踏み込むか。
// 単発のNAKでリセットするとゴーストキーの原因になるため1にはしないこと
#define IOEXP_FAIL_THRESHOLD 3

// 何回スキャンごとにMCP23017の設定レジスタの健全性を確認するか
// (IOEXP_SCAN_INTERVAL_MS = 10ms なので 50回 = 500ms間隔)
#define IOEXP_HEALTH_CHECK_SCANS 50

#define IOEXP_ADDR       0x20
#define IOEXP_IODIRA     0x00
#define IOEXP_IODIRB     0x01
#define IOEXP_IPOLA      0x02
#define IOEXP_IPOLB      0x03
#define IOEXP_GPINTENA   0x04
#define IOEXP_GPINTENB   0x05
#define IOEXP_DEFVALA    0x06
#define IOEXP_DEFVALB    0x07
#define IOEXP_INTCONA    0x08
#define IOEXP_INTCONB    0x09
#define IOEXP_IOCON      0x0a
// IOCONはBANK=0では0x0a/0x0b、BANK=1では0x05/0x15に現れる。
// BANKの状態が分からない場所から確実にBANK=0へ戻すために両方へ書く
#define IOEXP_IOCON_BANK1 0x05
#define IOEXP_GPPUA      0x0c
#define IOEXP_GPPUB      0x0d
#define IOEXP_INTFA      0x0e
#define IOEXP_INTFB      0x0f
#define IOEXP_INTCAPA    0x10
#define IOEXP_INTCAPB    0x11
#define IOEXP_GPIOA      0x12
#define IOEXP_GPIOB      0x13
#define IOEXP_OLATA      0x14
#define IOEXP_OLATB      0x15

#define CODE_NERD 0xf5
#define CODE_INS 0xf6
#define CODE_CTRL 0xf7
#define CODE_ALT 0xf8
#define CODE_RIGHT 0xf9
#define CODE_LEFT 0xfa
#define CODE_DOWN 0xfb
#define CODE_UP 0xfc
#define CODE_2NDFN 0xfd
#define CODE_CAPS 0xfe

typedef enum {
    button_push,
    button_release
} buttondir;

char ioexp_sl2bl(char code);
char ioexp_bl2tl(char code); // 記号の切り替え
void ioexp_init();
char ioexp_getchr();
uint32_t ioexp_getchr_available();
int ioexp_write_register(uint8_t reg, uint8_t value); // 0 = 成功, -1 = 失敗
int ioexp_read_register(uint8_t reg, uint8_t retval[1]); // 0 = 成功, -1 = 失敗
void ioexp_getchrinfo(); // キーのコードと押したか離したかの情報を渡す
void ioexp_task(); // キースキャンの実行。メインループから頻繁に呼ぶこと
short ioexp_getkey(short index); // 現在押下しているキーの配列からindex番目のキー情報を返す

extern volatile short g_en_shift, g_en_esc;

#ifdef __cplusplus
}
#endif

#endif

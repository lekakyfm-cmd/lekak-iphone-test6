#!/usr/bin/env python3
"""Synchronise dans les IPA iPhone les 361 copies validées sur Windows.
Le module de fusion ARM64 (fusions v1.4) est compilé séparément.
Aucun changement de code graphique.
"""
import json
import pathlib
import sys
import tempfile
import zipfile

MAPPING = """1=555 2=555 3=275 4=532 5=138 6=409 7=505 8=426 9=505 11=417 12=41 13=177 14=396 15=649 16=395 17=282 18=247 19=396 20=574 21=260 22=600 23=242 24=210 25=167 26=461 27=519 28=433 29=486 30=528 31=95 32=426 33=291 34=59 35=11 36=11 38=62 40=62 43=210 44=555 45=180 46=628 47=22 50=24 52=600 53=175 54=2 55=202 58=434 59=409 60=461 61=434 62=4 63=555 64=249 65=621 69=22 71=395 72=505 73=409 74=249 75=409 76=239 77=574 78=154 79=242 80=22 83=22 84=600 85=282 86=505 87=68 90=356 91=628 93=138 94=367 96=202 97=551 98=62 99=15 100=410 103=545 104=555 105=242 106=22 107=494 108=168 110=480 111=175 112=22 113=432 114=574 115=109 116=409 117=426 118=175 119=68 120=473 121=234 122=434 123=234 124=433 125=649 126=649 127=649 128=62 129=63 130=62 131=76 132=555 135=38 136=555 138=465 139=465 140=15 141=15 142=409 143=555 145=409 146=409 147=582 148=426 149=487 150=38 151=63 152=555 154=555 155=555 157=409 159=11 163=425 165=425 193=22 194=385 195=385 197=22 198=600 200=545 201=545 202=600 204=545 206=545 207=22 208=385 209=545 210=95 211=22 214=22 215=175 216=22 217=175 218=22 219=22 220=545 221=22 223=22 224=545 225=97 226=545 227=545 228=97 229=545 230=545 231=545 232=22 233=385 235=95 236=22 237=22 238=22 239=22 240=22 241=95 242=545 243=545 244=95 245=22 246=545 247=545 248=600 249=95 250=22 251=545 252=95 253=22 254=22 255=385 256=22 258=22 259=22 260=95 261=22 262=22 264=22 265=22 266=22 267=22 268=385 269=97 270=22 272=22 280=22 282=22 284=22 287=22 309=133 310=275 311=3 312=486 313=62 314=41 315=275 316=244 317=412 318=505 319=244 320=628 321=574 322=433 323=202 324=433 325=433 326=260 327=417 328=275 329=275 330=417 331=239 332=649 333=649 334=244 335=353 336=433 337=574 338=353 339=167 340=461 341=461 342=461 343=109 344=244 345=417 346=600 347=433 348=207 349=282 350=3 351=628 352=202 353=282 354=24 358=649 359=395 360=635 361=239 362=649 363=574 364=275 365=3 366=215 367=24 368=109 369=353 370=353 371=353 372=353 373=353 374=76 375=503 376=177 377=432 378=242 379=503 380=649 381=649 382=436 383=503 384=505 385=50 386=436 387=97 388=545 389=545 390=545 391=545 392=215 393=434 394=396 395=15 396=403 397=403 398=63 399=582 400=396 401=511 402=433 403=357 404=409 405=409 406=68 407=433 408=545 409=433 410=480 411=180 412=244 413=282 414=545 415=275 416=528 417=409 418=357 419=555 420=215 421=505 422=433 423=458 424=403 425=545 426=532 427=409 428=409 429=582 430=4 431=275 432=532 433=433 434=357 435=582 436=15 437=582 438=275 439=409 440=409 441=409 442=487 443=403 444=409 445=409 446=275 447=433 448=532 449=600 450=600 451=234 452=15 453=433 454=649 455=275 456=15 457=555 458=638 459=409 460=532 461=275 462=409 463=582 470=22 485=22 494=22 508=22 510=22"""
COPIES = {int(pair.split("=")[0]): int(pair.split("=")[1]) for pair in MAPPING.split()}
assert len(COPIES) == 361
APP = "Payload/LekakNative.app/mods/"
def patch(ipa):
    replacements = {}
    changes = {}
    with zipfile.ZipFile(ipa) as src:
        normal = APP + "Lekak/mod.json"
        variant = APP + "Lekak_Raretes_Test/mod.json"
        if normal not in src.namelist() or variant not in src.namelist():
            raise RuntimeError("Rarities and Lekak data must both be bundled for iOS fusion v1.4: "+str(ipa))
        for item in [normal, variant]:
            raw = src.read(item)
            doc = json.loads(raw)
            cards = doc["cards"]
            count = 0
            if item == normal:
                assert len(cards) == 519
                for index, copy in COPIES.items():
                    assert index != 0 and "copy" in cards[index]
                    if cards[index]["copy"] != copy:
                        cards[index]["copy"] = copy
                        count += 1
            else:
                assert len(cards) == 4960
                for tier in range(4):
                    for index, copy in COPIES.items():
                        base = index + 722
                        pos = tier * 1240 + base - 1
                        assert cards[pos].get("id") == "t%d-b%04d" % (tier+1,base)
                        if cards[pos]["copy"] != copy:
                            cards[pos]["copy"] = copy
                            count += 1
            replacements[item] = json.dumps(doc,ensure_ascii=False,separators=(",",":")).encode()
            changes[item] = count
    with tempfile.NamedTemporaryFile(prefix="lekak-ios-",suffix=".ipa",dir=ipa.parent,delete=False) as tmp:
        dest=pathlib.Path(tmp.name)
    try:
        with zipfile.ZipFile(ipa) as src, zipfile.ZipFile(dest,"w") as dst:
            for info in src.infolist():
                blob = replacements.get(info.filename)
                dst.writestr(info,blob if blob is not None else src.read(info.filename))
        dest.replace(ipa)
    except BaseException:
        dest.unlink(missing_ok=True)
        raise
    with zipfile.ZipFile(ipa) as src:
        assert src.testzip() is None
        a = json.loads(src.read(normal))["cards"]
        b = json.loads(src.read(variant))["cards"]
        for tier in range(4):
            for index, copy in COPIES.items():
                assert a[index]["copy"] == copy
                assert b[tier*1240+index+721]["copy"] == copy
    print(ipa,changes)
def main(directory):
    files=sorted(directory.glob("Lekak_iPhone_32_*.ipa"))
    if not files:
        raise SystemExit("No rebuilt v0.32 IPA found. Declining partial release.")
    for ipa in files:patch(ipa)
if __name__=="__main__":
    main(pathlib.Path(sys.argv[1]))

"""Sign bounded fixtures with temporary keys; run the real firmware consumer."""
import argparse
import base64
import copy
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--openssl',required=True)
    parser.add_argument('--manifest',type=Path,required=True)
    args=parser.parse_args()
    manifest=json.loads(args.manifest.read_text(encoding='utf-8'))
    digest=manifest['output_sha256']
    body={'schema':'orcmaps-catalog-v1','revision':1,'generated_at':'2026-10-19T00:00:00Z',
          'expires_at':'2026-11-19T00:00:00Z','packs':[{'manifest':manifest,'archive':{
              'bytes':manifest['size_bytes'],'sha256':digest,
              'urls':[f'https://maps.example.test/{digest}/world.pmtiles']}}]}
    now=int(datetime.datetime(2026,10,19,1,tzinfo=datetime.timezone.utc).timestamp())
    with tempfile.TemporaryDirectory(prefix='map-catalog-security-') as folder:
        root=Path(folder);private=root/'private.pem';public=root/'public.pem'
        subprocess.run([args.openssl,'genpkey','-algorithm','EC','-pkeyopt',
                        'ec_paramgen_curve:P-256','-out',str(private)],check=True,capture_output=True)
        subprocess.run([args.openssl,'pkey','-in',str(private),'-pubout','-out',str(public)],
                       check=True,capture_output=True)
        count=0
        def run(name,expected,mutate=None,raw=None,last=0,accepted='-',clock=True,tamper=False,fail_json=False):
            nonlocal count
            value=copy.deepcopy(body)
            if mutate:mutate(value)
            payload=raw if raw is not None else (json.dumps(value,sort_keys=True,separators=(',',':'),ensure_ascii=False)+'\n').encode()
            signature=subprocess.run([args.openssl,'dgst','-sha256','-sign',str(private)],
                                     input=payload,check=True,capture_output=True).stdout
            if tamper:payload+=b' '
            (root/'payload').write_bytes(payload)
            (root/'signature').write_bytes(base64.b64encode(signature)+b'\n')
            if accepted=='same':accepted=hashlib.sha256(payload).hexdigest()
            result=subprocess.run([str(args.exe.resolve()),str(root/'payload'),str(root/'signature'),
                                   str(public),expected,str(now),str(last),accepted,'1' if clock else '0',
                                   str(len(value['packs'])),'1' if fail_json else '0'],capture_output=True,text=True)
            if result.returncode:
                raise RuntimeError(f'{name}: exit={result.returncode} {result.stdout} {result.stderr}')
            count+=1;print('PASS',name)
        run('valid','none')
        run('json-allocation-failure','json',fail_json=True)
        run('signature-before-parsing','signature',raw=b'{broken',tamper=True)
        run('altered-payload','signature',tamper=True)
        run('unknown-clock','clock',clock=False)
        run('expired','expired',lambda x:x.update(expires_at='2026-10-19T01:00:00Z'))
        run('future-generation','future',lambda x:x.update(generated_at='2026-10-20T00:00:00Z'))
        run('older-revision','rollback',last=2,accepted='a'*64)
        run('same-revision-same-payload','none',last=1,accepted='same')
        run('same-revision-different-payload','rollback',last=1,accepted='a'*64)
        run('legacy-schema','schema',lambda x:x.update(schema='catalog-v1'))
        run('fractional-revision','revision',lambda x:x.update(revision=1.5))
        run('duplicate-json-key','json',raw=b'{"schema":"a","schema":"b"}')
        run('trailing-data','json',raw=json.dumps(body).encode()+b' false')
        run('escaped-nul','json',raw=json.dumps(body).replace('"display_name": "',r'"display_name": "\u0000').encode())
        run('unsupported-profile','manifest',lambda x:x['packs'][0]['manifest'].update(content_profile='unknown'))
        run('archive-size-mismatch','identity',lambda x:x['packs'][0]['archive'].update(bytes=1))
        run('archive-hash-mismatch','identity',lambda x:x['packs'][0]['archive'].update(sha256='b'*64))
        run('remote-destination','schema',lambda x:x['packs'][0]['archive'].update(destination='/settings'))
        run('duplicate-identity','identity',lambda x:x['packs'].append(copy.deepcopy(x['packs'][0])))
        run('excess-depth','json',raw=b'{"x":'+b'['*17+b'0'+b']'*17+b'}')
        run('excess-nodes','json',raw=b'{"x":['+b'0,'*4096+b'0]}')
        for name,url in [('http','http://maps.test/x'),('credentials','https://user:secret@maps.test/x'),
                         ('mutable','https://maps.test/latest.pmtiles'),
                         ('query',f'https://maps.test/{digest}/x.pmtiles?token=x'),
                         ('encoded-path',f'https://maps.test/{digest}/x%20y.pmtiles'),
                         ('traversal',f'https://maps.test/{digest}/../x.pmtiles')]:
            run('url-'+name,'url',lambda x,u=url:x['packs'][0]['archive'].update(urls=[u]))
        run('oversize','limit',raw=b' '*(128*1024+1))
        print(f'RESULT: {count} signed consumer cases PASS; temporary private key removed on exit')


if __name__=='__main__':main()

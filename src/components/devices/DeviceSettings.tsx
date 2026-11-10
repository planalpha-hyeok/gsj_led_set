import { useEffect, useMemo, useState } from 'react';
import { allCards, allControllers, brandLabel, categoryLabel } from '../../data';
import { effectivePortCapacity } from '../../core/portCapacity';
import { useProject } from '../../store/ProjectContext';
import type { BitDepth, Brand, ControllerSpec, ReceivingCardSpec } from '../../types';
import { SourceBadge } from '../ui/Badge';
import { NumberField, Segmented, SelectField } from '../ui/Field';
import { Icon } from '../ui/Icon';
import { Section } from '../ui/Section';
import { CustomDeviceDialog } from './CustomDeviceDialog';

/** 시리즈별로 묶어서 select 옵션 생성 */
function grouped<T extends { series: string; id: string; model: string }>(list: T[]) {
  const map = new Map<string, T[]>();
  list.forEach((x) => map.set(x.series, [...(map.get(x.series) ?? []), x]));
  return [...map.entries()];
}

/** 장비 선택: 제조사, 컨트롤러, 수신카드, 비트·주사율 */
export function DeviceSettings() {
  const { project: p, analysis: a, update } = useProject();
  const ctrl = a.controller;
  const card = a.card;
  const [brand, setBrand] = useState<Brand>(ctrl?.brand ?? 'novastar');
  const [dialog, setDialog] = useState(false);

  // 다른 프로젝트를 불러오면 제조사 선택도 맞춤
  useEffect(() => {
    if (ctrl) setBrand(ctrl.brand);
  }, [ctrl?.brand, p.id]);

  const ctrls = useMemo(() => allControllers(p).filter((c) => c.brand === brand), [p, brand]);
  const cards = useMemo(() => allCards(p).filter((c) => c.brand === brand), [p, brand]);

  const changeBrand = (b: Brand) => {
    setBrand(b);
    const c = allControllers(p).find((x) => x.brand === b);
    const r = allCards(p).find((x) => x.brand === b);
    update({ controllerId: c?.id ?? '', receivingCardId: r?.id ?? '' });
  };

  const addController = (c: ControllerSpec) => {
    update({ customControllers: [...p.customControllers, c], controllerId: c.id });
    setBrand(c.brand);
  };
  const addCard = (c: ReceivingCardSpec) => {
    update({ customCards: [...p.customCards, c], receivingCardId: c.id });
    setBrand(c.brand);
  };
  const removeCustom = () => {
    if (ctrl?.source === 'user') update({ customControllers: p.customControllers.filter((x) => x.id !== ctrl.id), controllerId: '' });
  };
  const removeCustomCard = () => {
    if (card?.source === 'user') update({ customCards: p.customCards.filter((x) => x.id !== card.id), receivingCardId: '' });
  };

  return (
    <>
      <Section title="장비" icon="cpu" aside={<button type="button" className="link-btn" onClick={() => setDialog(true)}>
        <Icon name="plus" size={13} /> 장비 추가</button>}>
        <Segmented value={brand} onChange={changeBrand}
          options={[{ value: 'novastar', label: 'NovaStar' }, { value: 'colorlight', label: 'Colorlight' }, { value: 'custom', label: '기타' }]} />

        <SelectField label="컨트롤러 / 송출기" value={p.controllerId} onChange={(v) => update({ controllerId: v })}>
          <option value="" disabled>선택…</option>
          {grouped(ctrls).map(([series, list]) => (
            <optgroup key={series} label={series}>
              {list.map((c) => <option key={c.id} value={c.id}>{c.model} · {c.ports}포트</option>)}
            </optgroup>
          ))}
        </SelectField>
        {ctrl && (
          <div className="spec-card">
            <div className="spec-head">
              <strong>{brandLabel[ctrl.brand]} {ctrl.model}</strong>
              <SourceBadge source={ctrl.source} />
            </div>
            <dl className="spec-list mono">
              <dt>분류</dt><dd>{categoryLabel[ctrl.category]}</dd>
              <dt>출력 포트</dt><dd>{ctrl.ports} × {ctrl.portType}</dd>
              <dt>포트당</dt><dd>{ctrl.pixelsPerPort ? ctrl.pixelsPerPort.toLocaleString() : '미확인'} px</dd>
              <dt>장비 총용량</dt><dd>{ctrl.maxPixels.toLocaleString()} px</dd>
              {ctrl.maxWidth && <><dt>최대 가로/세로</dt><dd>{ctrl.maxWidth} / {ctrl.maxHeight}</dd></>}
              <dt>현재 설정 포트당</dt><dd className="accent">{effectivePortCapacity(ctrl, p.bitDepth, p.frameRate).toLocaleString()} px</dd>
            </dl>
            {ctrl.note && <p className="spec-note">{ctrl.note}</p>}
            {ctrl.source === 'user' && <button type="button" className="link-btn danger" onClick={removeCustom}><Icon name="trash" size={13} /> 이 장비 삭제</button>}
          </div>
        )}

        <SelectField label="수신카드" value={p.receivingCardId} onChange={(v) => update({ receivingCardId: v })}>
          <option value="" disabled>선택…</option>
          {grouped(cards).map(([series, list]) => (
            <optgroup key={series} label={series}>
              {list.map((c) => <option key={c.id} value={c.id}>{c.model}</option>)}
            </optgroup>
          ))}
        </SelectField>
        {card && (
          <div className="spec-card">
            <div className="spec-head">
              <strong>{brandLabel[card.brand]} {card.model}</strong>
              <SourceBadge source={card.source} />
            </div>
            <dl className="spec-list mono">
              <dt>최대 부하</dt><dd>{card.loadText}</dd>
              <dt>픽셀 수</dt><dd>{card.maxPixels.toLocaleString()} px</dd>
            </dl>
            {card.note && <p className="spec-note">{card.note}</p>}
            {card.source === 'user' && <button type="button" className="link-btn danger" onClick={removeCustomCard}><Icon name="trash" size={13} /> 이 카드 삭제</button>}
          </div>
        )}

        <div className="grid-2">
          <Segmented label="입력 비트" value={p.bitDepth} onChange={(v: BitDepth) => update({ bitDepth: v })}
            options={[{ value: 8, label: '8bit' }, { value: 10, label: '10bit' }, { value: 12, label: '12bit' }]} />
          <NumberField label="주사율" value={p.frameRate} min={23} max={240} integer unit="Hz" onChange={(v) => update({ frameRate: v })} />
        </div>
      </Section>

      {dialog && <CustomDeviceDialog defaultBrand={brand} onClose={() => setDialog(false)}
        onAddController={addController} onAddCard={addCard} />}
    </>
  );
}

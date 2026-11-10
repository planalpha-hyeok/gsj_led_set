import { useState } from 'react';
import { newId } from '../../store/defaults';
import type { Brand, ControllerSpec, PortType, ReceivingCardSpec } from '../../types';
import { NumberField, Segmented, TextField } from '../ui/Field';
import { Modal } from '../ui/Modal';

interface Props {
  defaultBrand: Brand;
  onClose: () => void;
  onAddController: (c: ControllerSpec) => void;
  onAddCard: (c: ReceivingCardSpec) => void;
}

/** 목록에 없는 장비를 사양서 값으로 직접 추가 */
export function CustomDeviceDialog({ defaultBrand, onClose, onAddController, onAddCard }: Props) {
  const [kind, setKind] = useState<'controller' | 'card'>('controller');
  const [brand, setBrand] = useState<Brand>(defaultBrand);
  const [model, setModel] = useState('');
  const [ports, setPorts] = useState(4);
  const [portType, setPortType] = useState<PortType>('1G');
  const [ppp, setPpp] = useState(650000);
  const [maxPixels, setMaxPixels] = useState(2_300_000);
  const [maxW, setMaxW] = useState(0);
  const [maxH, setMaxH] = useState(0);
  const [cardW, setCardW] = useState(512);
  const [cardH, setCardH] = useState(384);

  const valid = model.trim().length > 0;

  const submit = () => {
    if (!valid) return;
    if (kind === 'controller') {
      onAddController({
        id: newId('user-ctrl'), brand, model: model.trim(), series: '사용자', category: 'sender',
        ports, portType, pixelsPerPort: ppp, maxPixels,
        maxWidth: maxW > 0 ? maxW : undefined, maxHeight: maxH > 0 ? maxH : undefined, source: 'user',
      });
    } else {
      onAddCard({
        id: newId('user-card'), brand, model: model.trim(), series: '사용자',
        maxPixels: cardW * cardH, loadText: `${cardW}×${cardH}`, source: 'user',
      });
    }
    onClose();
  };

  return (
    <Modal title="사용자 장비 추가" onClose={onClose}
      footer={<>
        <button type="button" className="btn" onClick={onClose}>취소</button>
        <button type="button" className="btn primary" disabled={!valid} onClick={submit}>추가</button>
      </>}>
      <div className="stack">
        <Segmented label="종류" value={kind} onChange={setKind}
          options={[{ value: 'controller', label: '컨트롤러 / 송출기' }, { value: 'card', label: '수신카드' }]} />
        <Segmented label="제조사 (호환성 확인용)" value={brand} onChange={setBrand}
          options={[{ value: 'novastar', label: 'NovaStar' }, { value: 'colorlight', label: 'Colorlight' }, { value: 'custom', label: '기타' }]} />
        <TextField label="모델명" value={model} onChange={setModel} />
        {kind === 'controller' ? (
          <div className="grid-2">
            <NumberField label="출력 포트 수" value={ports} min={1} max={64} integer onChange={setPorts} />
            <Segmented label="포트 종류" value={portType} onChange={setPortType}
              options={[{ value: '1G', label: '1G' }, { value: '5G', label: '5G' }]} />
            <NumberField label="포트당 픽셀 (8bit 60Hz)" value={ppp} min={1} integer unit="px" onChange={setPpp} />
            <NumberField label="장비 총 픽셀" value={maxPixels} min={1} integer unit="px" onChange={setMaxPixels} />
            <NumberField label="최대 가로 (없으면 0)" value={maxW} min={0} integer unit="px" onChange={setMaxW} />
            <NumberField label="최대 세로 (없으면 0)" value={maxH} min={0} integer unit="px" onChange={setMaxH} />
          </div>
        ) : (
          <div className="grid-2">
            <NumberField label="최대 가로" value={cardW} min={1} integer unit="px" onChange={setCardW} />
            <NumberField label="최대 세로" value={cardH} min={1} integer unit="px" onChange={setCardH} />
          </div>
        )}
        <p className="hint">값은 반드시 제조사 사양서 기준으로 입력하세요. 추가한 장비는 이 프로젝트 파일에 함께 저장됩니다.</p>
      </div>
    </Modal>
  );
}

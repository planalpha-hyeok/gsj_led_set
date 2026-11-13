import { Modal } from '../ui/Modal';

/** 브라우저가 설치 창을 바로 띄울 수 없을 때 직접 설치하는 방법 안내 */
export function InstallDialog({ onClose }: { onClose: () => void }) {
  const isFile = location.protocol === 'file:';
  return (
    <Modal title="앱 설치" onClose={onClose} width={460}
      footer={<button type="button" className="btn primary" onClick={onClose}>확인</button>}>
      {isFile ? (
        <p className="hint">파일로 연 경우에는 설치할 수 없습니다. 웹 주소(planalpha-hyeok.github.io/gsj_led_set)로 열어서 설치하세요.</p>
      ) : (
        <div className="install-steps">
          <p>이 브라우저에서는 설치 창을 바로 띄울 수 없습니다. 브라우저 메뉴에서 직접 추가하세요.</p>
          <dl>
            <dt>삼성 인터넷</dt><dd>하단 메뉴(≡) → <b>현재 페이지 추가</b> → <b>홈 화면</b></dd>
            <dt>크롬 (안드로이드)</dt><dd>오른쪽 위 ⋮ → <b>앱 설치</b> 또는 <b>홈 화면에 추가</b></dd>
            <dt>사파리 (아이폰·아이패드)</dt><dd>공유 버튼 → <b>홈 화면에 추가</b></dd>
            <dt>크롬·엣지 (PC)</dt><dd>주소창 오른쪽의 설치 아이콘 또는 메뉴 → <b>앱 설치</b></dd>
          </dl>
          <p className="hint">설치하면 홈 화면에서 앱처럼 열리고, 한 번 열어 둔 뒤에는 인터넷이 없어도 사용할 수 있습니다.</p>
        </div>
      )}
    </Modal>
  );
}

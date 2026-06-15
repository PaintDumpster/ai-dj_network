
type MainButtonsProps = {
    onPrevious: () => void;
    onContinue: () => void;
    onNext: () => void;
};

function MainButtons({
    onPrevious,
    onContinue,
    onNext}: MainButtonsProps) {
    return (
        <div className="buttons">
            <button onClick={onPrevious} className="nav-button">‹</button>
            <button onClick={onContinue} className="nav-button">SELECT</button>
            <button onClick={onNext} className="nav-button">›</button>
        </div>
    )
}

export default MainButtons